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
CLICK_DECLS

SimpleReorder::SimpleReorder()
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
SimpleReorder::initialize(ErrorHandler *)
{
    _held_packet = NULL;
    _packet_counter = 0;
    return 0;
}


Packet *
SimpleReorder::pull(int)
{
    // This loop will run until a return statement is reached
    while(1)
    {
        if (_held_packet == NULL)
        {
            Packet *p = input(0).pull();
            if (!_active || !p)
                return p;
            else 
            {
                if((click_random() & SAMPLING_MASK) > _sampling_prob)
                    return p;
                else
                {
                    _held_packet = p;
        	    _oldAnno = _held_packet->timestamp_anno();
	            _held_packet->timestamp_anno().assign_now();
                    _held_packet->timestamp_anno() += _timeout;
                    Packet *p = input(0).pull();
                    if(p)
                        _packet_counter++;
                    return input(0).pull();
                }
            }
        }
        else
        {
            if(_packet_counter >= _packets_to_wait || _held_packet->timestamp_anno() <= Timestamp::now())
            {
                Packet *p = _held_packet;
        	p->timestamp_anno() = _oldAnno;
	        _held_packet = NULL;
                _packet_counter = 0;
                return p;
            }
            else
            {
                Packet *p = input(0).pull();
                if(p)
                    _packet_counter++;
                return p;
            }
        }
    }
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
