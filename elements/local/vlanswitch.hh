#ifndef CLICK_VLANSWITCH_HH
#define CLICK_VLANSWITCH_HH
#include <click/element.hh>
CLICK_DECLS

/*
=c

VlanSwitch

=s paint

sends packet stream to output chosen per-packet

=d

PaintSwitch sends every incoming packet to one of its output ports --
specifically, output port number K, where K is the value of the
incoming packet's paint annotation.  If the paint annotation is set to
255, the packet is duplicated and sent to all outputs.  Since there
are only 256 different paint annotations, including the special value
of 255, PaintSwitch can have up to 255 outputs.  If there is no output
port K, the packet is dropped.

PaintSwitch uses the PAINT annotation by default, but the ANNO argument can
specify any one-byte annotation.

=a StaticSwitch, PullSwitch, RoundRobinSwitch, StrideSwitch, HashSwitch,
RandomSwitch, Paint, PaintTee */

class VlanSwitch : public Element { public:

    VlanSwitch() CLICK_COLD;

    const char *class_name() const		{ return "VlanSwitch"; }
    const char *port_count() const		{ return "1/-"; }
    const char *processing() const		{ return PUSH; }

    int configure(Vector<String> &conf, ErrorHandler *errh) CLICK_COLD;

    void push(int, Packet *);

  private:

    Vector<int> _vlans;

};

CLICK_ENDDECLS
#endif