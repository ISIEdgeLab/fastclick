// -*- mode: c++; c-basic-offset: 4 -*-
#ifndef CLICK_TARGETEDDROPLASTNK_HH
#define CLICK_TARGETEDDROPLASTNK_HH
#include <click/element.hh>
#include <click/atomic.hh>
#include <click/timestamp.hh>
#include <click/ipaddress.hh>
#include "targetedloss.hh"
CLICK_DECLS

/*
 * =c
 *
 * TargetedDropLastNofK([P, I<KEYWORDS>])
 *
 * =s classification
 *
 * Drops the last N of K packets destined to or from the specified prefixes.  That is,
 * given an interval of K packets, the last N will be dropped and K - N will be forwarded.
 * After K packets have passed, a new interval will start and the last N will be dropped again.
 * Only affects the packets of the given classifiers, SOURCE, DEST or PREFIX.  SOURCE prefix
 * will match a packets source address against the specified prefix.  DEST prefix will match
 * a packets destination address against the specified prefix.  If both are set, both must match.
 * PREFIX will match either destination or source address of the given packet.  PREFIX is
 * mutually exclusive with SOURCE and DEST.
 *
 * =d
 * 
 * Keyword arguments are:
 *
 * =over 8
 *
 * =item N I<P>
 *
 * Number N packets to drop, Default is 1.
 *
 * =item K I<P>
 *
 * Number K packets total in an interval.  Default is 100.
 *
 * =item ACTIVE
 *
 * Boolean. TargetedDropLastNofK is active or inactive; when inactive, it sends all
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
 * =h N read/write
 *
 * Returns or sets the N parameter
 *
 * =h K read/write
 *
 * Returns or sets the K parameter
 *
 * =h active read/write
 *
 * Makes the element active or inactive.
 *
 * =a RandomSample
 * */


class TargetedDropLastNofK : public Element { public:

    TargetedDropLastNofK() CLICK_COLD;

    const char *class_name() const		{ return "TargetedDropLastNofK"; }
    const char *port_count() const		{ return PORTS_1_1; }
    const char *processing() const		{ return PULL; }

    int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;
    int initialize(ErrorHandler *) CLICK_COLD;
    bool can_live_reconfigure() const		{ return true; }
    void add_handlers() CLICK_COLD;

    Packet *pull(int port);

  private:


    enum { h_n, h_k, h_drops, h_config, h_source, h_dest, h_prefix };

    uint32_t _N;                        // N Parameter
    uint32_t _K;                        // K Parameter
    uint32_t _drops;                    // drop count
    uint32_t _packet_count;             // Packets dropped
    bool _sampling;                     // Currently Sampling or currently bursting?
    bool _active;


    // Using the tl_prefix struct from TargetedLoss.hh, perhaps make a copy here?
    tl_prefix _source;
    tl_prefix _dest;
    tl_prefix _prefix;

    bool _source_set;
    bool _dest_set;

    static String read_handler(Element *, void *) CLICK_COLD;
    static int drop_write_handler(const String &, Element *, void *, ErrorHandler *) CLICK_COLD;
    static int prefix_write_handler(const String &, Element *, void*, ErrorHandler *) CLICK_COLD;
    
};

CLICK_ENDDECLS
#endif
