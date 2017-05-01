// -*- mode: c++; c-basic-offset: 4 -*-
/*
 * simplereorder.{cc,hh} -- element probabilistically hold/reorder packets
 * Erik Kline
 *
 * Copyright (c) 2016 University of Southern California
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, subject to the conditions
 * listed in the Click LICENSE file. These conditions include: you must
 * preserve this copyright notice, and you cannot mention the copyright
 * holders in advertising related to the Software without their permission.
 * The Software is provided WITHOUT ANY WARRANTY, EXPRESS OR IMPLIED. This
 * notice is a summary of the Click LICENSE file; the license in that file is
 * legally binding.
 */

#include <click/config.h>
#include "simplereorder.hh"
#include <click/args.hh>
#include <click/straccum.hh>
#include <click/error.hh>
#include <click/timestamp.hh>
#include <click/standard/scheduleinfo.hh>
#include <click/packet_anno.hh>
CLICK_DECLS

SimpleReorder::SimpleReorder()
    : _task(this), _timer(&_task)
{
}

int
SimpleReorder::configure(Vector<String> &conf, ErrorHandler *errh)
{
    uint32_t sampling_prob = 0xFFFFFFFFU;
    bool active = true;
    uint32_t packets = 1;
    Timestamp timeout = Timestamp::make_msec(0, 1);
    if (Args(conf, this, errh)
	.read_p("P", FixedPointArg(SAMPLING_SHIFT), sampling_prob)
	.read("PACKETS", packets)
	.read("TIMEOUT", timeout)
	.read("ACTIVE", active)
	.complete() < 0)
	return -1;

    // OK: set variables
    _sampling_prob = sampling_prob;
    _active = active;
    _packets_to_wait = packets;
    _timeout = timeout;
    
    
    return 0;
}

int
SimpleReorder::initialize(ErrorHandler *errh)
{
    _head = NULL;
    _tail = NULL;
    _packet_counter = 0;

    lock = false;
    ScheduleInfo::initialize_task(this, &_task, errh);
    _timer.initialize(this);

    return 0;
}

// Relative sampling!

Packet*
SimpleReorder::emit()
{
    Packet* p = _head;
    _head = p->next();
    
    if(_head == NULL)
    {
        _tail = NULL;
    }
    else
    {
        SET_AGGREGATE_ANNO(_head, AGGREGATE_ANNO(p) - AGGREGATE_ANNO(_head));
    }
    
    p->set_next(NULL);
    SET_AGGREGATE_ANNO(p, (uint32_t) 0);
    return p;
}
    
Packet *
SimpleReorder::pull(int)
{
    while(lock);
    lock = true;
    // This loop will run until a return statement is reached
    while(1)
    {
        Timestamp now = Timestamp::now();

        // Check if we're holding any packets
        if (_head != NULL)
        {
            // Check if it's time to emit a held packet
            if(AGGREGATE_ANNO(_head) >= _packets_to_wait || FIRST_TIMESTAMP_ANNO(_head) <= now)
            {
                // It's time, update the queue
                Packet* p = emit();
                lock = false;
                return p;
            }
        }
        
        // Grab a packet and make a decision
        Packet *p = input(0).pull();
        if (!_active || !p)
        {
            lock = false;
            return p;
        }
        else 
        {            
            if((click_random() & SAMPLING_MASK) > _sampling_prob)
            {
                if(_head)
                {
                    SET_AGGREGATE_ANNO(_head, AGGREGATE_ANNO(_head) + 1);
                    _packet_counter++;
                }
                lock = false;
                return p;
            }
            else
            {
                if(_tail == NULL)
                {
                    _tail = p;
                    _head = _tail;
                    // Kernel version does not support multiply
                    // so using add instead
                    Timestamp resched = now + _timeout;
                    _timer.schedule_at(resched);
                }
                else
                {
                    _tail->set_next(p);
                    _tail = p;
                }
                _tail->set_next(NULL);
                FIRST_TIMESTAMP_ANNO(_tail).assign_now();
                FIRST_TIMESTAMP_ANNO(_tail) += _timeout;
                SET_AGGREGATE_ANNO(_tail, _packet_counter);
                _packet_counter = 0;
            }
        }
    } 
}

bool
SimpleReorder::run_task(Task *)
{
    bool worked = false;
    Timestamp now = Timestamp::now();

    
    if(!lock)
    {
        lock = true;
        Packet *p = _head;
        if(p != NULL)
        {
            if(FIRST_TIMESTAMP_ANNO(p) <= now)
            {
                p = emit();
                checked_output_push(1, p);
                worked = true;
            }
            
        }
        lock = false;

    }
    if(_head)
    {

        Timestamp resched = FIRST_TIMESTAMP_ANNO(_head);
        _timer.schedule_at(resched);
    }
    return worked;
}


String
SimpleReorder::read_handler(Element *e, void *thunk)
{
    SimpleReorder *sr = static_cast<SimpleReorder *>(e);
    switch ((intptr_t)thunk) {
      case h_sample:
        return cp_unparse_real2(sr->_sampling_prob, SAMPLING_SHIFT);
      case h_packets: {
        StringAccum sa;
        sa << sr->_packets_to_wait;
        return sa.take_string();
      }
      case h_timeout:
        return sr->_timeout.unparse();
      case h_config: {
	StringAccum sa;
	sa << "SAMPLE " << cp_unparse_real2(sr->_sampling_prob, SAMPLING_SHIFT)
           << ", PACKETS " << sr->_packets_to_wait
           << ", TIMEOUT " << sr->_timeout
           << ", ACTIVE " << sr->_active;
	return sa.take_string();
      }
      default:
	return "<error>";
    }
}

int
SimpleReorder::prob_write_handler(const String &str, Element *e, void *thunk, ErrorHandler *errh)
{
    SimpleReorder *sr = static_cast<SimpleReorder *>(e);
    uint32_t p;
    if (!FixedPointArg(SAMPLING_SHIFT).parse(cp_uncomment(str), p) || p > (1 << SAMPLING_SHIFT))
	return errh->error("Must be given a number between 0.0 and 1.0");
    sr->_sampling_prob = p;
    return 0;
}

int
SimpleReorder::timeout_write_handler(const String &str, Element *e, void *thunk, ErrorHandler *errh)
{
    SimpleReorder *sr = static_cast<SimpleReorder *>(e);
    if (!cp_time(str, &sr->_timeout))
        return errh->error("timeout must be a timestamp");
    return 0;
}
    
void
SimpleReorder::add_handlers()
{
    add_read_handler("sampling_prob", read_handler, h_sample);
    add_write_handler("sampling_prob", prob_write_handler, h_sample);
    add_data_handlers("active", Handler::OP_READ | Handler::OP_WRITE | Handler::CHECKBOX, &_active);
    add_data_handlers("packets", Handler::OP_READ | Handler::OP_WRITE, &_packets_to_wait);
    add_read_handler("timeout", read_handler, h_timeout);
    add_write_handler("timeout", timeout_write_handler, h_timeout);
    add_read_handler("config", read_handler, h_config);
    set_handler_flags("config", 0, Handler::CALM);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(SimpleReorder)
ELEMENT_MT_SAFE(SimpleReorder)
