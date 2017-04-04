// -*- c-basic-offset: 4 -*-
/*
 * delayshaper.{cc,hh} -- element pulls packets from input, delays returnign
 * the packet to output port.
 *
 * Copyright (c) 1999-2001 Massachusetts Institute of Technology
 * Copyright (c) 2003 International Computer Science Institute
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
#include <click/error.hh>
#include <click/args.hh>
#include <click/glue.hh>
#include <click/straccum.hh>
#include "delayjittershaper.hh"
#include <click/standard/scheduleinfo.hh>
CLICK_DECLS

DelayJitterShaper::DelayJitterShaper()
    : _p(0), _timer(this), _notifier(Notifier::SEARCH_CONTINUE_WAKE)
{
}

void *
DelayJitterShaper::cast(const char *n)
{
    if (strcmp(n, "DelayJitterShaper") == 0)
	return (DelayJitterShaper *)this;
    else if (strcmp(n, Notifier::EMPTY_NOTIFIER) == 0)
	return &_notifier;
    else
	return Element::cast(n);
}

int
DelayJitterShaper::configure(Vector<String> &conf, ErrorHandler *errh)
{
    unsigned int burst = 1;
    Timestamp jitter = Timestamp(0);
    _notifier.initialize(Notifier::EMPTY_NOTIFIER, router());
    if (Args(conf, this, errh).read_mp("DELAY", _delay).read("JITTER", jitter).read("BURST", burst).complete() < 0)
	return -1;
    _burst = burst;
    _jitter = jitter;
    _currentJitter = jitter;
    click_chatter(_jitter.unparse_interval().c_str());   
    _count = 1;
    return 0;
    
}

int
DelayJitterShaper::initialize(ErrorHandler *)
{
    _timer.initialize(this);
    _upstream_signal = Notifier::upstream_empty_signal(this, 0, &_notifier);
    return 0;
}

void
DelayJitterShaper::cleanup(CleanupStage)
{
    if (_p)
	_p->kill();
}

Packet *
DelayJitterShaper::pull(int)
{
    // read a packet
    if (!_p && (_p = input(0).pull())) {
	if (!_p->timestamp_anno().sec()) // get timestamp if not set
	    _p->timestamp_anno().assign_now();
	if (_jitter.usecval() != 0 && _count >= _burst)
	{
	   _count = 0;
	   _currentJitter = Timestamp::make_usec(click_random(0, _jitter.usecval()));
	}	   
	_p->timestamp_anno() += _delay + _currentJitter;
    }

    if (_p) {
	Timestamp now = Timestamp::now();
	if (_p->timestamp_anno() <= now) {
	    // packet ready for output
	    Packet *p = _p;
	    p->timestamp_anno() = now;
	    _p = 0;
	    _count = _count + 1;
	    return p;
	}

	// adjust time by a bit
	Timestamp expiry = _p->timestamp_anno() - Timer::adjustment();
	if (expiry <= now) {
	    // small delta, don't go to sleep -- but mark our Signal as active,
	    // since we have something ready.
	    _notifier.wake();
	} else {
	    // large delta, go to sleep and schedule Timer
	    _timer.schedule_at(expiry);
	    _notifier.sleep();
	}
    } else if (!_upstream_signal) {
	// no packet available, we go to sleep right away
	_notifier.sleep();
    }

    return 0;
}

void
DelayJitterShaper::run_timer(Timer *)
{
    _notifier.wake();
}

String
DelayJitterShaper::read_param(Element *e, void *thunk)
{
    DelayJitterShaper *u = (DelayJitterShaper *)e;
    switch ((intptr_t) thunk) {
    case h_delay:
	return u->_delay.unparse_interval();
    case h_jitter:
	return u->_jitter.unparse_interval();
    case h_burst:
	{
	    StringAccum sa;
	    sa << u->_burst;
	    return sa.take_string();
	}
    default:
	return "<error>";
    }
    return "<error>";
}

int
DelayJitterShaper::write_param(const String &s, Element *e, void *thunk, ErrorHandler *errh)
{
    DelayJitterShaper *u = (DelayJitterShaper *)e;
    switch ((intptr_t) thunk) {
    case h_delay:
	if (!cp_time(s, &u->_delay))
	    return errh->error("delay must be a timestamp");
	return 0;
    case h_jitter:
	if (!cp_time(s, &u->_jitter))
	    return errh->error("jitter must be a timestamp");
	return 0;
    case h_burst:
	if (!cp_integer(s, &u->_burst))
	    return errh->error("burst must be a valid integer");
	return 0;
    default:
	return errh->error("Unknown config parameter");
    }
}

void
DelayJitterShaper::add_handlers()
{
    add_read_handler("delay", read_param, h_delay, Handler::h_calm);
    add_read_handler("jitter", read_param, h_jitter, Handler::h_calm);
    add_read_handler("burst", read_param, h_burst, Handler::h_calm);
    add_write_handler("delay", write_param, h_delay, Handler::h_nonexclusive);
    add_write_handler("jitter", write_param, h_jitter, Handler::h_nonexclusive);
    add_write_handler("burst", write_param, h_burst, Handler::h_nonexclusive);

}

CLICK_ENDDECLS
EXPORT_ELEMENT(DelayJitterShaper)
ELEMENT_MT_SAFE(DelayJitterShaper)
