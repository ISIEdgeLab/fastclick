// -*- mode: c++; c-basic-offset: 4 -*-
#ifndef CLICK_TARGETEDLOSS_HH
#define CLICK_TARGETEDLOSS_HH
#include <click/element.hh>
#include <click/atomic.hh>
#include <click/timestamp.hh>
#include <click/ipaddress.hh>
CLICK_DECLS

/*
 * =c
 *
 * SimpleReorder([P, I<KEYWORDS>])
 *
 * =s classification
 *
 * samples packets with some probability, holds it and emits it later
 *
 * =d
 *
 * Samples packets with probability P. The sampled packet is held while 
 * other packets are emitted.  The sampled packet is emitted after N packets
 * or T time has passed,
 *
 *
 * Keyword arguments are:
 *
 * =over 8
 *
 * =item PACKETS I<P>
 *
 * Number of packets to emit before the sampled packet
 *
 * =item TIMEOUT I<Q>
 *
 * Time to wait before emitting the sampled packet
 *
 * =item ACTIVE
 *
 * Boolean. TargetedLoss is active or inactive; when inactive, it sends all
 * packets to output 0. Default is true (active).
 *
 * =back
 *
 * =h sample_prob read/write
 *
 * Returns or sets the sampling probability.
 *
 * =h packets read/write
 *
 * Returns or sets the number of packets to emit before sampled packet
 *
 * =h timeout read/write
 *
 * Returns or sets the timeout before emitting the sampled packet
 *
 * =h active read/write
 *
 * Makes the element active or inactive.
 *
 * =a RandomSample
 * */

struct tl_prefix
{
    IPAddress net;
    IPAddress mask;
};

class TargetedLoss : public Element { public:

    TargetedLoss() CLICK_COLD;

    const char *class_name() const		{ return "TargetedLoss"; }
    const char *port_count() const		{ return PORTS_1_1; }
    const char *processing() const		{ return PULL; }

    int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;
    int initialize(ErrorHandler *) CLICK_COLD;
    bool can_live_reconfigure() const		{ return true; }
    void add_handlers() CLICK_COLD;

    Packet *pull(int port);

  private:

    enum { SAMPLING_SHIFT = 28 };
    enum { SAMPLING_MASK = (1 << SAMPLING_SHIFT) - 1 };

    enum { h_sample, h_drops, h_config, h_source, h_dest, h_prefix };

    uint32_t _sampling_prob;		// out of (1<<SAMPLING_SHIFT)
    uint32_t _burst;                    // # Packets before emitting held packet
    uint32_t _drops;
    uint32_t _packet_count;
    bool _sampling;                     // Currently Sampling or currently bursting?
    bool _active;

    tl_prefix _source;
    tl_prefix _dest;
    tl_prefix _prefix;

    bool _source_set;
    bool _dest_set;

    static String read_handler(Element *, void *) CLICK_COLD;
    static int prob_write_handler(const String &, Element *, void *, ErrorHandler *) CLICK_COLD;
    static int drop_write_handler(const String &, Element *, void *, ErrorHandler *) CLICK_COLD;
    static int prefix_write_handler(const String &, Element *, void*, ErrorHandler *) CLICK_COLD;
    
};

CLICK_ENDDECLS
#endif
