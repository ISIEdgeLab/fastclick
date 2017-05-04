// -*- mode: c++; c-basic-offset: 4 -*-
/*
 * targeteddroplastnofk.{cc,hh} -- element to drop last N of K packets.
 * Erik Kline
 *
 * Copyright (c) 2017 University of Southern California
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
#include "targeteddroplastNofK.hh"
#include <click/args.hh>
#include <click/straccum.hh>
#include <click/error.hh>
#include <click/timestamp.hh>
#include <clicknet/ip.h>
CLICK_DECLS

TargetedDropLastNofK::TargetedDropLastNofK()
{
    
}

int
TargetedDropLastNofK::configure(Vector<String> &conf, ErrorHandler *errh)
{
    bool active = true;
    uint32_t n = 1;
    uint32_t k = 100;
    
    IPAddress source = IPAddress(0);
    IPAddress smask = IPAddress(0);
    IPAddress dest = IPAddress(0);
    IPAddress dmask = IPAddress(0);
    IPAddress prefix = IPAddress(0);
    IPAddress pmask = IPAddress(0);

    _source_set = _dest_set = false;

    if (Args(conf, this, errh)
	.read("N", n)
        .read("K", k)
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

    if (n > k)
    {
        errh->error("Cannot set N %u to be greater than K %u", n, k);
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

    _N = n;
    _K = k;
    _packet_count = _K;
    _active = active;

#ifdef DEBUGGING_TARGET_DROP_FIRST_N
    click_chatter("N %d, K %d, Active %d", _N, _K, _active);
    click_chatter("Source %s/%s, Dest %s/%s, Prefix %s/%s", _source.net.unparse().c_str(),  _source.mask.unparse().c_str(),  _dest.net.unparse().c_str(), _dest.mask.unparse().c_str(), _prefix.net.unparse().c_str(), _prefix.mask.unparse().c_str());
#endif
    
    return 0;
}

int
TargetedDropLastNofK::initialize(ErrorHandler *)
{
    _drops = 0;
    return 0;
}


Packet *
TargetedDropLastNofK::pull(int)
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
        // Count the packet since it matches
        _packet_count--;
        // Check if we're in the last _N portion or in the _K - _N portion
        if(_packet_count <= _N && _packet_count > 0)
        {
            // In the _N portion, drop the packets.
            p->kill();
            _drops++;
            return NULL;
        }
        if((signed) _packet_count <= 0)
        {
            // Have we seen _K packets, if so, reset count and return to _N portion
            _packet_count = _K;
        }
        click_chatter("%d", _packet_count);
    }
    // We made it here, return the unsampled packet.
    return p;
}

String
TargetedDropLastNofK::read_handler(Element *e, void *thunk)
{
    TargetedDropLastNofK *tdlnk = static_cast<TargetedDropLastNofK *>(e);
    switch ((intptr_t)thunk) {
      case h_config: {
	StringAccum sa;
	sa << "N " << tdlnk->_N << ", K " << tdlnk->_K;

        if(tdlnk->_source_set || tdlnk->_dest_set)
        {
            if(tdlnk->_source_set)
            {
                sa << ", SOURCE " << tdlnk->_source.net.unparse() << "/" << tdlnk->_source.mask.unparse() << " ";
            }
            if(tdlnk->_dest_set)
            {
                sa << ", DEST " << tdlnk->_dest.net.unparse() << "/" << tdlnk->_dest.mask.unparse() << " ";
            }
        }
        else
        {
            sa << ", PREFIX " << tdlnk->_prefix.net.unparse() << "/" << tdlnk->_prefix.mask.unparse() <<  " ";
        }
            
	return sa.take_string();
      }
      case h_source: {
        StringAccum sa;
        if(tdlnk->_source_set)
        {
            sa << "SOURCE " << tdlnk->_source.net.unparse() << "/" << tdlnk->_source.mask.unparse();
        }
        else
        {
            sa << "Source not set";
        }
        return sa.take_string();
      }
      case h_dest: {
        StringAccum sa;
        if(tdlnk->_dest_set)
        {
            sa << "DEST " << tdlnk->_dest.net.unparse() << "/" << tdlnk->_dest.mask.unparse();
        }
        else
        {
            sa << "Dest not set";
        }
        return sa.take_string();
      }
      case h_prefix: {
        StringAccum sa;
        if(tdlnk->_source_set || tdlnk->_dest_set)
        {
            sa << "Source or Dest set!";
        }
        else
        {
            sa << "PREFIX " << tdlnk->_prefix.net.unparse() << "/" << tdlnk->_prefix.mask.unparse();
        }
        return sa.take_string();
              
      }
      default:
	return "<error>";
    }
    
}

int
TargetedDropLastNofK::prefix_write_handler(const String &str, Element *e, void *thunk, ErrorHandler *errh)
{
    TargetedDropLastNofK *tdlnk = static_cast<TargetedDropLastNofK *>(e);
    IPAddress net;
    IPAddress mask;
    tdlnk->_packet_count = tdlnk->_K;
    switch ((intptr_t)thunk) {
      case h_prefix:
          if(!IPPrefixArg(true).parse(cp_uncomment(str), net, mask))
              return errh->error("Invalid prefix %s", str.c_str());
          tdlnk->_source_set = false;
          tdlnk->_dest_set = false;
          tdlnk->_prefix.net = net;
          tdlnk->_prefix.mask = mask;
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
          tdlnk->_source_set = true;
          tdlnk->_source.net = net;
          tdlnk->_source.mask = mask;

          if(clear_dest)
          {
              tdlnk->_dest_set = false;
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
          tdlnk->_dest_set = true;
          tdlnk->_dest.net = net;
          tdlnk->_dest.mask = mask;

          if(clear_source)
          {
              tdlnk->_source_set = false;
          }
          return 0;
      }
      default:
          return 0;
    }
}


int
TargetedDropLastNofK::drop_write_handler(const String &, Element* e, void*, ErrorHandler*)
{
    TargetedDropLastNofK *tdlnk = static_cast<TargetedDropLastNofK *>(e);
    tdlnk->_drops = 0;
}
    
void
TargetedDropLastNofK::add_handlers()
{
    add_data_handlers("active", Handler::OP_READ | Handler::OP_WRITE | Handler::CHECKBOX, &_active);
    add_data_handlers("N", Handler::OP_READ | Handler::OP_WRITE, &_N);
    add_data_handlers("K", Handler::OP_READ | Handler::OP_WRITE, &_K);
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
EXPORT_ELEMENT(TargetedDropLastNofK)
ELEMENT_MT_SAFE(TargetedDropLastNofK)
