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
 * TargetedLoss([P, I<KEYWORDS>])
 *
 * =s classification
 *
 * Drops packets destined to or from the specified prefixes.  Will also drop packets in bursts.
 * Several keywords are available.  BURST sets the number of packets to drop when dropping. 
 * (default 1)  That is, the element will roll the dice for a given packet and if within the given
 * drop probability, will enter dropping mode.  Then, it will drop BURST packets, including the 
 * current packet.  The packets affected will match one or more classifiers.  Options are SOURCE,
 * DEST, or PREFIX, all of which expect a prefix (or an IP address).  SOURCE and DEST can both
 * be specified to different prefixes.  Packets source address or destination address will be
 * matched against the specified prefixes.  The PREFIX option is mutually exclusive to the SOURCE
 * or DEST option.  The Prefix option will match to either source or destination address.  The 
 * default prefix is 0.0.0.0/0, that is, it will match everything.
 *
 * =d
 *
 * Drops packets with probability P
 * *
 * Keyword arguments are:
 *
 * =over 8
 *
 * =item BURST I<P>
 *
 * Number of packets to drop when dropping.  Default is 1.
 *
 * =item ACTIVE
 *
 * Boolean. TargetedLoss is active or inactive; when inactive, it sends all
 * packets to output 0. Default is true (active).
 *
 * =item SOURCE
 *
 * IP Prefix or Address.  Will match packets source address against the specified prefix.  
 * Mutually exclusive with the PREFIX keyword.  Default is 0.0.0.0/0.
 *
 * =item DEST
 *
 * IP Prefix or Address.  Will match packets destination address against the specified prefix.  
 * Mutually exclusive with the PREFIX keyword.  Default is 0.0.0.0/0.
 *
 * =item PREFIX
 *
 * IP Prefix or Address.  Will match packets source or destination address against the 
 * specified prefix. Mutually exclusive with the SOURCE and DEST  keywords.  Default is 0.0.0.0/0.
 *
 * =back
 *
 * =h drop_prob read/write
 *
 * Returns or sets the dropping probability.
 *
 * =h source read/write
 *
 * Returns the source prefix if set.  Writes the SOURCE prefix.  This will unset the PREFIX
 * parameter if set.  Also takes an optional parameter CLEAROTHER which, if true, will clear
 * the DEST setting.
 *
 * =h dest read/write
 *
 * Returns the dest prefix if set.  Writes the DEST prefix.  This will unset the PREFIX
 * parameter if set.  Also takes an optional parameter CLEAROTHER which, if true, will clear
 * the SOURCE setting.
 *
 * =h prefix read/write
 *
 * Returns the prefix if set.  Writes the PREFIX.  This will unset the SOURCE and DEST prefixes.
 *
 * =h drops read
 *
 * Returns the count of dropped packets
 *
 * =h burst read/write
 *
 * Returns or sets the burst of packets to drop 
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
    uint32_t _burst;                    // Burst drop threshold
    uint32_t _drops;                    // drop count
    uint32_t _packet_count;             // Packets dropped
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
