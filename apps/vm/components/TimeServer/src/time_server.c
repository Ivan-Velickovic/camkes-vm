/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the GNU General Public License version 2. Note that NO WARRANTY is provided.
 * See "LICENSE_GPLv2.txt" for details.
 *
 * @TAG(NICTA_GPL)
 */

#include <autoconf.h>

#include <assert.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdio.h>
#include <sel4/sel4.h>
#include <sel4/arch/constants.h>
#include <TimeServer.h>
#include <platsupport/plat/hpet.h>
#include <platsupport/arch/tsc.h>
#include "vm.h"
#include <boost/preprocessor/repeat.hpp>
#include <utils/math.h>

#define MAX_CLIENT_TIMERS 9

/* Prototype for this function is not generated by the camkes templates yet */
seL4_Word the_timer_get_badge();

#ifdef CONFIG_APP_CAMKES_VM_HPET_MSI
#define TIMER_IRQ (MSI_MIN + IRQ_OFFSET) //24 + IRQ_OFFSET
#else
#define TIMER_IRQ HPET_IRQ()
#endif

/* Frequency of timer interrupts that we use for processing timeouts */
#define TIMER_FREQUENCY 500

static pstimer_t *timer = NULL;

#define TIMER_TYPE_OFF 0
#define TIMER_TYPE_PERIODIC 1
#define TIMER_TYPE_ABSOLUTE 2
#define TIMER_TYPE_RELATIVE 3

typedef struct client_timer {
    int id;
    int client_id;
    int timer_type;
    uint64_t periodic_ns;
    uint64_t timeout_time;
    struct client_timer *prev, *next;
} client_timer_t;

typedef struct client_state {
    int id;
    uint32_t completed;
    client_timer_t timers[MAX_CLIENT_TIMERS];
} client_state_t;

/* sorted list of active timers */
static client_timer_t *timer_head = NULL;

/* declare the memory needed for the clients */
static client_state_t client_state[VM_NUM_TIMER_CLIENTS];

static uint64_t tsc_frequency = 0;

#define TIMER_COMPLETE_EMIT_OUTPUT(a, vm, b) BOOST_PP_CAT(BOOST_PP_CAT(timer, BOOST_PP_INC(vm)),_complete_emit),
static void (*timer_complete_emit[])(void) = {
    BOOST_PP_REPEAT(VM_NUM_TIMERS, TIMER_COMPLETE_EMIT_OUTPUT, _)
};

static uint64_t current_time_ns() {
    return muldivu64(rdtsc_pure(), NS_IN_S, tsc_frequency);
}

static void remove_timer(client_timer_t *timer) {
    if (timer->prev) {
        timer->prev->next = timer->next;
    } else {
        assert(timer == timer_head);
        timer_head = timer->next;
    }
    if (timer->next) {
        timer->next->prev = timer->prev;
    }
}

static void insert_timer(client_timer_t *timer) {
    client_timer_t *current, *next;
    for (current = NULL, next = timer_head; next && next->timeout_time < timer->timeout_time; current = next, next = next->next);
    timer->prev = current;
    timer->next = next;
    if (next) {
        next->prev = timer;
    }
    if (current) {
        current->next = timer;
    } else {
        timer_head = timer;
    }
}

static void signal_client(client_timer_t *timer, uint64_t current_time) {
    timer_complete_emit[timer->client_id]();
    client_state[timer->client_id].completed |= BIT(timer->id);
    remove_timer(timer);
    switch(timer->timer_type) {
    case TIMER_TYPE_OFF:
        assert(!"not possible");
        break;
    case TIMER_TYPE_PERIODIC:
        timer->timeout_time += timer->periodic_ns;
        insert_timer(timer);
        break;
    case TIMER_TYPE_ABSOLUTE:
    case TIMER_TYPE_RELATIVE:
        timer->timer_type = TIMER_TYPE_OFF;
        break;
    }
}

static void signal_clients(uint64_t current_time) {
    while(timer_head && timer_head->timeout_time <= current_time) {
        signal_client(timer_head, current_time);
    }
}

static void timer_interrupt(void *cookie) {
    time_server_lock();
    signal_clients(current_time_ns());
    timer_handle_irq(timer, TIMER_IRQ);
    irq_reg_callback(timer_interrupt, cookie);
    time_server_unlock();
}

static int _oneshot_relative(int cid, int tid, uint64_t ns) {
    if (tid >= MAX_CLIENT_TIMERS || tid < 0) {
        return -1;
    }
    time_server_lock();
    client_timer_t *t = &client_state[cid].timers[tid];
    if (t->timer_type != TIMER_TYPE_OFF) {
        remove_timer(t);
    }
    t->timer_type = TIMER_TYPE_RELATIVE;
    t->timeout_time = current_time_ns() + ns;
    insert_timer(t);
    time_server_unlock();
    return 0;
}

static int _oneshot_absolute(int cid, int tid, uint64_t ns) {
    if (tid >= MAX_CLIENT_TIMERS || tid < 0) {
        return -1;
    }
    time_server_lock();
    client_timer_t *t = &client_state[cid].timers[tid];
    if (t->timer_type != TIMER_TYPE_OFF) {
        remove_timer(t);
    }
    t->timer_type = TIMER_TYPE_ABSOLUTE;
    t->timeout_time = ns;
    insert_timer(t);
    time_server_unlock();
    return 0;
}

static int _periodic(int cid, int tid, uint64_t ns) {
    if (tid >= MAX_CLIENT_TIMERS || tid < 0) {
        return -1;
    }
    time_server_lock();
    client_timer_t *t = &client_state[cid].timers[tid];
    if (t->timer_type != TIMER_TYPE_OFF) {
        remove_timer(t);
    }
    t->timer_type = TIMER_TYPE_PERIODIC;
    t->periodic_ns = ns;
    t->timeout_time = current_time_ns() + ns;
    insert_timer(t);
    time_server_unlock();
    return 0;
}

static int _stop(int cid, int tid) {
    if (tid >= MAX_CLIENT_TIMERS || tid < 0) {
        return -1;
    }
    time_server_lock();
    client_timer_t *t = &client_state[cid].timers[tid];
    if (t->timer_type != TIMER_TYPE_OFF) {
        remove_timer(t);
        t->timer_type = TIMER_TYPE_OFF;
    }
    time_server_unlock();
    return 0;
}

static unsigned int _completed(int cid) {
    unsigned int ret;
    time_server_lock();
    ret = client_state[cid].completed;
    client_state[cid].completed = 0;
    time_server_unlock();
    return ret;
}

static uint64_t _time(int cid) {
    uint64_t ret;
    ret = current_time_ns();
    return ret;
}

/* substract 1 from the badge as we started counting badges at 1
 * to avoid using the 0 badge */
int the_timer_oneshot_relative(int id, uint64_t ns) {
    return _oneshot_relative(the_timer_get_badge() - 1, id, ns);
}

int the_timer_oneshot_absolute(int id, uint64_t ns) {
    return _oneshot_absolute(the_timer_get_badge() - 1, id, ns);
}

int the_timer_periodic(int id, uint64_t ns) {
    return _periodic(the_timer_get_badge() - 1, id, ns);
}

int the_timer_stop(int id) {
    return _stop(the_timer_get_badge() - 1, id);
}

unsigned int the_timer_completed() {
    return _completed(the_timer_get_badge() - 1);
}

uint64_t the_timer_time() {
    return _time(the_timer_get_badge() - 1);
}

uint64_t the_timer_tsc_frequency() {
    return tsc_frequency;
}

void post_init() {
    time_server_lock();
//    set_putchar(putchar_putchar);
    for (int i = 0; i < VM_NUM_TIMER_CLIENTS; i++) {
        client_state[i].id = i;
        client_state[i].completed = 0;
        for (int j = 0; j < MAX_CLIENT_TIMERS; j++) {
            client_state[i].timers[j] =
                (client_timer_t) {.id = j, .client_id = i, .timer_type = TIMER_TYPE_OFF, .prev = NULL, .next = NULL};
        }
    }
    int ioapic;
#ifdef CONFIG_APP_CAMKES_VM_HPET_MSI
    ioapic = 0;
#else
    ioapic = 1;
#endif
    hpet_config_t config = (hpet_config_t){.vaddr = (void*)hpet, .irq = TIMER_IRQ, .ioapic_delivery = ioapic};
    timer = hpet_get_timer(&config);
    assert(timer);
    tsc_frequency = tsc_calculate_frequency(timer);
    assert(tsc_frequency);
    irq_reg_callback(timer_interrupt, NULL);
    /* start timer */
    timer_start(timer);
    timer_periodic(timer, NS_IN_S / TIMER_FREQUENCY);
    time_server_unlock();
}
