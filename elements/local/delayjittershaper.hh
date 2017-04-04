// -*- c-basic-offset: 4 -*-
#ifndef CLICK_DELAYJITTERSHAPER_HH
#define CLICK_DELAYJITTERSHAPER_HH
#include <click/element.hh>
#include <click/timer.hh>
#include <click/notifier.hh>
#include "../standard/delayshaper.hh"
CLICK_DECLS

/*
=c

DelayJitterShaper(DELAY, JITTER <J>, BURST <B>)

=s shaping

shapes traffic to meet delay requirements with Jitter

=d

Pulls packets from the single input port. Delays them for at least DELAY
seconds, with microsecond precision. A packet with timestamp T will be emitted
no earlier than time (T + DELAY + JITTER). On output, the packet's timestamp 
is set to the current time.

SetTimestamp element can be used to stamp the packet.

=h delay read/write

Returns or sets the DELAY parameter.

=h jitter read/write

Returns or sets the JITTER parameter

=h burst read/write

Returns or sets the Burst parameter

=a BandwidthShaper, DelayUnqueue, SetTimestamp */

class DelayJitterShaper : public DelayShaper {
public:

    DelayJitterShaper() CLICK_COLD;

    const char *class_name() const	{ return "DelayJitterShaper"; }
    const char *port_count() const	{ return PORTS_1_1; }
    const char *processing() const	{ return PULL; }
    void *cast(const char *);

    int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;
    int initialize(ErrorHandler *) CLICK_COLD;
    void cleanup(CleanupStage) CLICK_COLD;
    void add_handlers() CLICK_COLD;

    Packet *pull(int);
    void run_timer(Timer *);

  private:

    enum { h_delay, h_jitter, h_burst };
    
    Packet *_p;
    Timestamp _delay;
    Timestamp _jitter;
    Timestamp _currentJitter;
    int _burst;
    int _count;
    Timer _timer;
    NotifierSignal _upstream_signal;
    ActiveNotifier _notifier;

    static String read_param(Element *, void *) CLICK_COLD;
    static int write_param(const String &, Element *, void *, ErrorHandler *) CLICK_COLD;

};

CLICK_ENDDECLS
#endif
