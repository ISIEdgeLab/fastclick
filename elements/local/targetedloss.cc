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
#include "targetedloss.hh"
#include <click/args.hh>
#include <click/straccum.hh>
#include <click/error.hh>
#include <click/timestamp.hh>
#include <clicknet/ip.h>
CLICK_DECLS

TargetedLoss::TargetedLoss()
{
    
}

int
TargetedLoss::configure(Vector<String> &conf, ErrorHandler *errh)
{
    uint32_t sampling_prob = 0xFFFFFFFFU;
    bool active = true;
    uint32_t burst = 1;
    
    IPAddress source = IPAddress(0);
    IPAddress smask = IPAddress(0);
    IPAddress dest = IPAddress(0);
    IPAddress dmask = IPAddress(0);
    IPAddress prefix = IPAddress(0);
    IPAddress pmask = IPAddress(0);

    _source_set = _dest_set = false;

    if (Args(conf, this, errh)
	.read_p("P", FixedPointArg(SAMPLING_SHIFT), sampling_prob)
	.read("BURST", burst)
	.read("SOURCE", IPPrefixArg(true), source, smask)
        .read("DEST", IPPrefixArg(true), dest, dmask)
        .read("PREFIX", IPPrefixArg(true), prefix, pmask)
	.read("ACTIVE", active)
	.complete() < 0)
	return -1;

    if ((source != IPAddress(0) || dest != IPAddress(0)) && prefix != IPAddress(0))
    {
        errh->error("Cannot set both global prefix and source/dest prefixes\n");
        return -1;
    }

    if(source != IPAddress(0) || dest != IPAddress(0))
    {
        if(source != IPAddress(0))
        {
            _source_set = true;
            _source.net = source;
            _source.mask = smask;
        }
        if(dest != IPAddress(0))
        {
            _dest_set = true;
            _dest.net = dest;
            _dest.mask = dmask;
        }
    }
    else
    {
        _prefix.net = prefix;
        _prefix.mask = pmask;
    }
        
    // OK: set variables
    _sampling_prob = sampling_prob;
    _active = active;
    _burst = burst;

#ifdef DEBUGGING_TARGET_LOSS
    click_chatter("Burst %d, Active %d, Probability %s", _burst, _active, cp_unparse_real2(_sampling_prob, SAMPLING_SHIFT).c_str());
    click_chatter("Source %s/%s, Dest %s/%s, Prefix %s/%s", _source.net.unparse().c_str(),  _source.mask.unparse().c_str(),  _dest.net.unparse().c_str(), _dest.mask.unparse().c_str(), _prefix.net.unparse().c_str(), _prefix.mask.unparse().c_str());
#endif
    
    return 0;
}

int
TargetedLoss::initialize(ErrorHandler *)
{
    _sampling = false;
    _packet_count = 0;
    _drops = 0;
    return 0;
}


Packet *
TargetedLoss::pull(int)
{
    Packet* p = input(0).pull();

    // Check to see if we're doing anything or if we got a bad packet
    if(!_active || !p)
        return p;

    // Check to see if our header is annotated since we're going to rely on that.
    if(!p->has_network_header())
    {
        click_chatter("No network header set!  Consider using a CheckIPHeader element");
        return p;
    }

    // Get the IP header and pull source and dest addresses in network byte order
    const click_ip* ip_header = p->ip_header();
    IPAddress p_src = IPAddress(ip_header->ip_src);
    IPAddress p_dst = IPAddress(ip_header->ip_dst);

    bool match = false;

    // If we're doing source and/or dest matching
    if(_source_set || _dest_set)
    {
        // Either we're doing only source or dest matching, or we're doing both.
        // This logic should work in both cases.  If we're not doing source matching, then the first
        // clause will be true and we must be doing dest matching so we'll check the dest clause.
        // Vice versa if doing source and not dest.  In the both case, we check the second part
        // of both clauses.
        if((!_source_set || p_src.matches_prefix(_source.net, _source.mask)) &&
           (!_dest_set || p_dst.matches_prefix(_dest.net, _dest.mask)))
        {
            match = true;
        }
    }
    else if(p_src.matches_prefix(_prefix.net, _prefix.mask) ||
            p_dst.matches_prefix(_prefix.net, _prefix.mask))
    {
        // We're doing prefix matching, only source or dest needs to match.
        match = true;
    }

    // Our packet matches, yay!
    if(match)
    {
        // If we're not already sampling, check if we should be sampling
        if(!_sampling && (click_random() & SAMPLING_MASK) <= _sampling_prob)
        {
            _sampling = true;
        }
        if(_sampling)
        {
            // We're sampling, kill the packet, and update the burst counter.
            p->kill();
            _drops++;
            _packet_count++;
            if(_packet_count >= _burst)
            {
                _sampling = false;
                _packet_count = 0;
            }
            return NULL;
        }
    }
    // We made it here, return the unsampled packet.
    return p;
}

String
TargetedLoss::read_handler(Element *e, void *thunk)
{
    TargetedLoss *tl = static_cast<TargetedLoss *>(e);
    switch ((intptr_t)thunk) {
      case h_sample:
        return cp_unparse_real2(tl->_sampling_prob, SAMPLING_SHIFT);
      case h_config: {
	StringAccum sa;
	sa << "SAMPLE " << cp_unparse_real2(tl->_sampling_prob, SAMPLING_SHIFT)
           << ", BURST " << tl->_burst;

        if(tl->_source_set || tl->_dest_set)
        {
            if(tl->_source_set)
            {
                sa << ", SOURCE " << tl->_source.net.unparse() << "/" << tl->_source.mask.unparse() << " ";
            }
            if(tl->_dest_set)
            {
                sa << ", DEST " << tl->_dest.net.unparse() << "/" << tl->_dest.mask.unparse() << " ";
            }
        }
        else
        {
            sa << ", PREFIX " << tl->_prefix.net.unparse() << "/" << tl->_prefix.mask.unparse() <<  " ";
        }
            
	return sa.take_string();
      }
      case h_source: {
        StringAccum sa;
        if(tl->_source_set)
        {
            sa << "SOURCE " << tl->_source.net.unparse() << "/" << tl->_source.mask.unparse();
        }
        else
        {
            sa << "Source not set";
        }
        return sa.take_string();
      }
      case h_dest: {
        StringAccum sa;
        if(tl->_dest_set)
        {
            sa << "DEST " << tl->_dest.net.unparse() << "/" << tl->_dest.mask.unparse();
        }
        else
        {
            sa << "Dest not set";
        }
        return sa.take_string();
      }
      case h_prefix: {
        StringAccum sa;
        if(tl->_source_set || tl->_dest_set)
        {
            sa << "Source or Dest set!";
        }
        else
        {
            sa << "PREFIX " << tl->_prefix.net.unparse() << "/" << tl->_prefix.mask.unparse();
        }
        return sa.take_string();
              
      }
      default:
	return "<error>";
    }
    
}

int
TargetedLoss::prefix_write_handler(const String &str, Element *e, void *thunk, ErrorHandler *errh)
{
    TargetedLoss *tl = static_cast<TargetedLoss *>(e);
    IPAddress net;
    IPAddress mask;
    switch ((intptr_t)thunk) {
      case h_prefix:
          if(!IPPrefixArg(true).parse(cp_uncomment(str), net, mask))
              return errh->error("Invalid prefix %s", str.c_str());
          tl->_source_set = false;
          tl->_dest_set = false;
          tl->_prefix.net = net;
          tl->_prefix.mask = mask;
          return 0;
      case h_source: {
          Vector<String> args;
          bool clear_dest = false;
          cp_argvec(str, args);
          if (Args(args, e, errh)
              .read_mp("SOURCE", IPPrefixArg(true), net, mask)
              .read("CLEAROTHER", clear_dest)
              .complete() < 0)
              return -1;
          tl->_source_set = true;
          tl->_source.net = net;
          tl->_source.mask = mask;

          if(clear_dest)
          {
              tl->_dest_set = false;
          }
          return 0;
      }
      case h_dest: {
          Vector<String> args;
          bool clear_source = false;
          cp_argvec(str, args);
          if (Args(args, e, errh)
              .read_mp("DEST", IPPrefixArg(true), net, mask)
              .read("CLEAROTHER", clear_source)
              .complete() < 0)
              return -1;
          tl->_dest_set = true;
          tl->_dest.net = net;
          tl->_dest.mask = mask;

          if(clear_source)
          {
              tl->_source_set = false;
          }
          return 0;
      }
      default:
          return 0;
    }
}


int
TargetedLoss::prob_write_handler(const String &str, Element *e, void *thunk, ErrorHandler *errh)
{
    TargetedLoss *tl = static_cast<TargetedLoss *>(e);
    uint32_t p;
    if (!FixedPointArg(SAMPLING_SHIFT).parse(cp_uncomment(str), p) || p > (1 << SAMPLING_SHIFT))
	return errh->error("Must be given a number between 0.0 and 1.0");
    tl->_sampling_prob = p;
    return 0;
}

int
TargetedLoss::drop_write_handler(const String &, Element* e, void*, ErrorHandler*)
{
    TargetedLoss *tl = static_cast<TargetedLoss *>(e);
    tl->_drops = 0;
}
    
void
TargetedLoss::add_handlers()
{
    add_read_handler("sampling_prob", read_handler, h_sample);
    add_write_handler("sampling_prob", prob_write_handler, h_sample);
    add_data_handlers("active", Handler::OP_READ | Handler::OP_WRITE | Handler::CHECKBOX, &_active);
    add_data_handlers("burst", Handler::OP_READ | Handler::OP_WRITE, &_burst);
    add_data_handlers("drops", Handler::OP_READ, &_drops);
    add_write_handler("clear_drops", drop_write_handler, h_drops);
    add_write_handler("source", prefix_write_handler, h_source);
    add_read_handler("source", read_handler, h_source);
    add_write_handler("dest", prefix_write_handler, h_dest);
    add_read_handler("dest", read_handler, h_dest);
    add_write_handler("prefix", prefix_write_handler, h_prefix);
    add_read_handler("prefix", read_handler, h_prefix);
    add_read_handler("config", read_handler, h_config);
    set_handler_flags("config", 0, Handler::CALM);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(TargetedLoss)
ELEMENT_MT_SAFE(TargetedLoss)
