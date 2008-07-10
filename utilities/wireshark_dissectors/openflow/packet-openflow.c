/**
 * Filename: packet-openflow.c
 * Author:   David Underhill
 * Updated:  2008-Jul-09
 * Purpose:  define a Wireshark 0.99.x-1.x dissector for the OpenFlow protocol
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <glib.h>
#include <epan/packet.h>
#include <epan/prefs.h>
#include <string.h>
#include <arpa/inet.h>
#include <openflow.h>

#define PROTO_TAG_OPENFLOW	"OPENFLOW"

/* Wireshark ID of the OPENFLOW protocol */
static int proto_openflow = -1;
static dissector_handle_t openflow_handle;
static void dissect_openflow(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree);

/* traffic will arrive with TCP port OPENFLOW_DST_TCP_PORT */
#define UDP_PORT_FILTER "tcp.port"
static int global_openflow_proto = OPENFLOW_DST_TCP_PORT;

/* AM=Async message, CSM=Control/Switch Message */
/** names to bind to various values in the type field */
static const value_string names_type[] = {
    { OFPT_FEATURES_REQUEST,    "CSM: Features Request" },
    { OFPT_FEATURES_REPLY,      "CSM: Feautres Reply" },
    { OFPT_GET_CONFIG_REQUEST,  "CSM: Get Config Request" },
    { OFPT_GET_CONFIG_REPLY,    "CSM: Get Config Reply" },
    { OFPT_SET_CONFIG,          "CSM: Set Config" },
    { OFPT_PACKET_IN,           "AM:  Packet In" },
    { OFPT_PACKET_OUT,          "CSM: Packet Out" },
    { OFPT_FLOW_MOD,            "CSM: Flow Mod" },
    { OFPT_FLOW_EXPIRED,        "AM:  Flow Expired" },
    { OFPT_TABLE,               "CSM: Table" },
    { OFPT_PORT_MOD,            "CSM: Port Mod" },
    { OFPT_PORT_STATUS,         "AM:  Port Status" },
    { OFPT_STATS_REQUEST,       "CSM: Stats Request" },
    { OFPT_STATS_REPLY,         "CSM: Stats Reply" },
    { OFPT_ERROR_MSG,           "AM:  Error Message" },
    { 0,                        NULL }
};

/* The hf_* variables are used to hold the IDs of our header fields; they are
 * set when we call proto_register_field_array() in proto_register_openflow()
 */
static gint hf_of                     = -1;
static gint hf_of_header              = -1;
static gint hf_of_pad                 = -1;
static gint hf_of_version             = -1;
static gint hf_of_num_events          = -1;
static gint hf_of_seq                 = -1;
static gint hf_of_queue_size_words[8] = {-1,-1,-1,-1,-1,-1,-1,-1};
static gint hf_of_queue_size_pkts[8]  = {-1,-1,-1,-1,-1,-1,-1,-1};

static gint hf_of_event               = -1;
static gint hf_of_type                = -1;
static gint hf_of_time_full           = -1;
static gint hf_of_time_top            = -1;
static gint hf_of_time_btm            = -1;

static gint hf_of_short_event         = -1;
static gint hf_of_queue_id            = -1;
static gint hf_of_packet_len          = -1;
static gint hf_of_time_lsb            = -1;

/* These are the ids of the subtrees that we may be creating */
static gint ett_of                     = -1;
static gint ett_of_header              = -1;
static gint ett_of_pad                 = -1;
static gint ett_of_version             = -1;
static gint ett_of_num_events          = -1;
static gint ett_of_seq                 = -1;
static gint ett_of_queue_size_words[8] = {-1,-1,-1,-1,-1,-1,-1,-1};
static gint ett_of_queue_size_pkts[8]  = {-1,-1,-1,-1,-1,-1,-1,-1};

static gint ett_of_event               = -1;
static gint ett_of_type                = -1;
static gint ett_of_time_full           = -1;
static gint ett_of_time_top            = -1;
static gint ett_of_time_btm            = -1;

static gint ett_of_short_event         = -1;
static gint ett_of_queue_id            = -1;
static gint ett_of_packet_len          = -1;
static gint ett_of_time_lsb            = -1;

void proto_reg_handoff_openflow()
{
    openflow_handle = create_dissector_handle(dissect_openflow, proto_openflow);
    dissector_add(TCP_PORT_FILTER, global_openflow_proto, openflow_handle);
}

#define NO_STRINGS NULL
#define NO_MASK 0x0

void proto_register_openflow()
{
    /* A header field is something you can search/filter on.
    *
    * We create a structure to register our fields. It consists of an
    * array of hf_register_info structures, each of which are of the format
    * {&(field id), {name, abbrev, type, display, strings, bitmask, blurb, HFILL}}.
    */
    static hf_register_info hf[] = {
        /* header fields */
        { &hf_of,
          { "Data", "of.data",                FT_NONE,   BASE_NONE, NO_STRINGS,       NO_MASK,      "NF2 Event Capture PDU",          HFILL }},

        { &hf_of_header,
          { "Header", "of.header",            FT_NONE,   BASE_NONE, NO_STRINGS,       NO_MASK,      "NF2 Event Capture Header",       HFILL }},

        { &hf_of_pad,
          { "Padding", "of.pad",              FT_UINT8,  BASE_DEC,  NO_STRINGS,       MASK_PAD,     "Padding",                        HFILL }},

        { &hf_of_version,
          { "Version", "of.ver",              FT_UINT8,  BASE_DEC,  NO_STRINGS,       MASK_VERSION, "Version",                        HFILL }},

        { &hf_of_num_events,
          { "# of Events", "of.num_events",   FT_UINT8,  BASE_DEC,  NO_STRINGS,       NO_MASK,      "# of Events",                    HFILL }},

        { &hf_of_seq,
          { "Seq #", "of.seq",                FT_UINT32, BASE_DEC,  NO_STRINGS,       NO_MASK,      "Sequence #",                     HFILL }},

        { &hf_of_queue_size_words[0],
          { "CPU0  Words  ", "of.cpu0w",      FT_UINT32, BASE_DEC,  NO_STRINGS,       NO_MASK,      "CPU0 Size in 64-bit Words",      HFILL }},

        { &hf_of_queue_size_pkts[0],
          { "CPU0  Packets", "of.cpu0p",      FT_UINT32, BASE_DEC,  NO_STRINGS,       NO_MASK,      "CPU0 Size in Packets",           HFILL }},

        { &hf_of_queue_size_words[1],
          { "NF2C0 Words  ", "of.nf2c0w",     FT_UINT32, BASE_DEC,  NO_STRINGS,       NO_MASK,      "NF2C0 Size in 64-bit Words",     HFILL }},

        { &hf_of_queue_size_pkts[1],
          { "NF2C0 Packets", "of.nf2c0p",     FT_UINT32, BASE_DEC,  NO_STRINGS,       NO_MASK,      "NF2C0 Size in Packets",          HFILL }},

        { &hf_of_queue_size_words[2],
          { "CPU1  Words  ", "of.cpu1w",      FT_UINT32, BASE_DEC,  NO_STRINGS,       NO_MASK,      "CPU1 Size in 64-bit Words",      HFILL }},

        { &hf_of_queue_size_pkts[2],
          { "CPU1  Packets", "of.cpu1p",      FT_UINT32, BASE_DEC,  NO_STRINGS,       NO_MASK,      "CPU1 Size in Packets",           HFILL }},

        { &hf_of_queue_size_words[3],
          { "NF2C1 Words  ", "of.nf2c1w",     FT_UINT32, BASE_DEC,  NO_STRINGS,       NO_MASK,      "NF2C1 Size in 64-bit Words",     HFILL }},

        { &hf_of_queue_size_pkts[3],
          { "NF2C1 Packets", "of.nf2c1p",     FT_UINT32, BASE_DEC,  NO_STRINGS,       NO_MASK,      "NF2C1 Size in Packets",          HFILL }},

        { &hf_of_queue_size_words[4],
          { "CPU2  Words  ", "of.cpu2w",      FT_UINT32, BASE_DEC,  NO_STRINGS,       NO_MASK,      "CPU2 Size in 64-bit Words",      HFILL }},

        { &hf_of_queue_size_pkts[4],
          { "CPU2  Packets", "of.cpu2p",      FT_UINT32, BASE_DEC,  NO_STRINGS,       NO_MASK,      "CPU2 Size in Packets",           HFILL }},

        { &hf_of_queue_size_words[5],
          { "NF2C2 Words  ", "of.nf2c2w",     FT_UINT32, BASE_DEC,  NO_STRINGS,       NO_MASK,      "NF2C2 Size in 64-bit Words",     HFILL }},

        { &hf_of_queue_size_pkts[5],
          { "NF2C2 Packets", "of.nf2c2p",     FT_UINT32, BASE_DEC,  NO_STRINGS,       NO_MASK,      "NF2C2 Size in Packets",          HFILL }},

        { &hf_of_queue_size_words[6],
          { "CPU3  Words  ", "of.cpu3w",      FT_UINT32, BASE_DEC,  NO_STRINGS,       NO_MASK,      "CPU3 Size in 64-bit Words",      HFILL }},

        { &hf_of_queue_size_pkts[6],
          { "CPU3  Packets", "of.cpu3p",      FT_UINT32, BASE_DEC,  NO_STRINGS,       NO_MASK,      "CPU3 Size in Packets",           HFILL }},

        { &hf_of_queue_size_words[7],
          { "NF2C3 Words  ", "of.nf2c3w",     FT_UINT32, BASE_DEC,  NO_STRINGS,       NO_MASK,      "NF2C3 Size in 64-bit Words",     HFILL }},

        { &hf_of_queue_size_pkts[7],
          { "NF2C3 Packets", "of.nf2c3p",     FT_UINT32, BASE_DEC,  NO_STRINGS,       NO_MASK,      "NF2C3 Size in Packets",          HFILL }},


        /* event type fields */
        { &hf_of_event,
          { "Event", "of.event",              FT_NONE,   BASE_NONE, NO_STRINGS,       NO_MASK,      "Event",                          HFILL }},

        { &hf_of_type,
          { "Type", "of.type",                FT_UINT32, BASE_DEC,  VALS(names_type), MASK_TYPE,    "Event Type",                     HFILL }},

        /* note: this takes advantage that the type is 0, therefore the upper
           two bits in the timestamp will be 0 and can be safely included as part
           of the timestamp */
        { &hf_of_time_full,
          { "Timestamp", "of.ts",             FT_STRING, BASE_NONE, NO_STRINGS,       NO_MASK,      "Timestamp in units of 8ns",      HFILL }},

        { &hf_of_time_top,
          { "Timestamp Upper", "of.ts_top",   FT_UINT32, BASE_DEC,  NO_STRINGS,       MASK_TIME_TOP, "Upper Timestamp in units of 8ns", HFILL }},

        { &hf_of_time_btm,
          { "Timestamp Lower", "of.ts_btm",   FT_UINT32, BASE_DEC,  NO_STRINGS,       NO_MASK,       "Lower Timestamp in units of 8ns", HFILL }},

        { &hf_of_short_event,
          { "Event", "of.ev",                FT_STRING, BASE_NONE, NO_STRINGS,       NO_MASK,      "Short Event",                      HFILL }},

        { &hf_of_queue_id,
          { "Queue", "of.q",                  FT_UINT32, BASE_DEC,  VALS(names_queue_id), MASK_QUEUE_ID,  "Queue",                      HFILL }},

        { &hf_of_packet_len,
          { "Packet Length", "of.len",        FT_UINT32, BASE_DEC,  NO_STRINGS,       MASK_PACKET_LEN, "Packet Length (B)",           HFILL }},

        { &hf_of_time_lsb,
          { "Timestamp (LSB)", "of.ts_lsb",   FT_UINT32, BASE_DEC,  NO_STRINGS,       MASK_TIME_LSB,  "Timestamp (LSB) in untis of 8ns", HFILL }}
    };

    static gint *ett[] = {
        &ett_of,
        &ett_of_header,
        &ett_of_pad,
        &ett_of_version,
        &ett_of_num_events,
        &ett_of_seq,
        &ett_of_queue_size_words[0],
        &ett_of_queue_size_pkts[0],
        &ett_of_queue_size_words[1],
        &ett_of_queue_size_pkts[1],
        &ett_of_queue_size_words[2],
        &ett_of_queue_size_pkts[2],
        &ett_of_queue_size_words[3],
        &ett_of_queue_size_pkts[3],
        &ett_of_queue_size_words[4],
        &ett_of_queue_size_pkts[4],
        &ett_of_queue_size_words[5],
        &ett_of_queue_size_pkts[5],
        &ett_of_queue_size_words[6],
        &ett_of_queue_size_pkts[6],
        &ett_of_queue_size_words[7],
        &ett_of_queue_size_pkts[7],

        &ett_of_event,
        &ett_of_type,
        &ett_of_time_full,
        &ett_of_time_top,
        &ett_of_time_btm,

        &ett_of_short_event,
        &ett_of_queue_id,
        &ett_of_packet_len,
        &ett_of_time_lsb
    };

    proto_openflow = proto_register_protocol( "NetFPGA Event Capture Protocol",
                                              "OPENFLOW",
                                              "of" ); /* abbreviation for filters */

    proto_register_field_array (proto_openflow, hf, array_length (hf));
    proto_register_subtree_array (ett, array_length (ett));
    register_dissector("openflow", dissect_openflow, proto_openflow);
}

/**
 * Adds "hf" to "tree" starting at "offset" into "tvb" and using "length"
 * bytes.  offset is incremented by length.
 */
static void add_child( proto_item* tree, gint hf, tvbuff_t *tvb, guint32* offset, guint32 len ) {
    proto_tree_add_item( tree, hf, tvb, *offset, len, FALSE );
    *offset += len;
}

/**
 * Adds "hf" to "tree" starting at "offset" into "tvb" and using "length" bytes.
 */
static void add_child_const( proto_item* tree, gint hf, tvbuff_t *tvb, guint32 offset, guint32 len ) {
    proto_tree_add_item( tree, hf, tvb, offset, len, FALSE );
}

static void
dissect_openflow(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree)
{
    /* display our protocol text if the protocol column is visible */
    if (check_col(pinfo->cinfo, COL_PROTOCOL))
        col_set_str(pinfo->cinfo, COL_PROTOCOL, PROTO_TAG_OPENFLOW);

    /* Clear out stuff in the info column */
    if(check_col(pinfo->cinfo,COL_INFO)){
        col_clear(pinfo->cinfo,COL_INFO);
    }

    /* get some of the header fields' values for later use */
    guint8  ver = tvb_get_guint8( tvb, 0 ) & MASK_VERSION;
    guint32 seq = tvb_get_ntohl( tvb, 2 );
    guint32 ts_top = tvb_get_ntohl( tvb, 70 );
    guint32 ts_btm = tvb_get_ntohl( tvb, 74 );
    char*   str_ts = timestamp8ns_to_string( ts_top, ts_btm);

    /* clarify protocol name display with version, sequence number, and timestamp */
    if (check_col(pinfo->cinfo, COL_INFO)) {
        col_add_fstr( pinfo->cinfo, COL_INFO, "NF2 Update v%u (seq=%u, time=%s)", ver, seq, str_ts );
    }

    if (tree) { /* we are being asked for details */
        proto_item *item        = NULL;
        proto_item *sub_item    = NULL;
        proto_tree *of_tree     = NULL;
        proto_tree *header_tree = NULL;
        guint32 offset = 0;

        /* consume the entire tvb for the openflow packet, and add it to the tree */
        item = proto_tree_add_item(tree, proto_openflow, tvb, 0, -1, FALSE);
        of_tree = proto_item_add_subtree(item, ett_of);
        header_tree = proto_item_add_subtree(item, ett_of);

        /* put the header in its own node as a child of the openflow node */
        sub_item = proto_tree_add_item( of_tree, hf_of_header, tvb, offset, -1, FALSE );
        header_tree = proto_item_add_subtree(sub_item, ett_of_header);

        /* add the headers field as children of the header node */
        add_child_const( header_tree, hf_of_pad,     tvb, offset, 1 );
        add_child_const( header_tree, hf_of_version, tvb, offset, 1 );
        offset += 1;
        add_child( header_tree, hf_of_num_events,    tvb, &offset, 1 );
        add_child( header_tree, hf_of_seq,           tvb, &offset, 4 );
        guint8 i;
        for( i=0; i<8; i++ ) {
            add_child( header_tree, hf_of_queue_size_words[i], tvb, &offset, 4 );
            add_child( header_tree, hf_of_queue_size_pkts[i],  tvb, &offset, 4 );
        }

        /* add the timestamp (computed the string representation earlier) */
        proto_tree_add_string( header_tree, hf_of_time_full, tvb, offset, 8, str_ts );
        offset += 8;

        /** handle events (loop until out of bytes) */
        while( offset <= MAX_EC_SIZE - 4 ) {
            /* get the 2-bit type field */
            guint8 type = (tvb_get_guint8( tvb, offset ) & 0xC0) >> 6;

            if( type == TYPE_TIMESTAMP ) {
                if( offset > MAX_EC_SIZE - 8 )
                    break;

                ts_top = tvb_get_ntohl( tvb, offset );
                ts_btm = tvb_get_ntohl( tvb, offset+4 );
                str_ts = timestamp8ns_to_string( ts_top, ts_btm );
                proto_tree_add_string( of_tree, hf_of_short_event, tvb, offset, 8, event_to_string(0, 0, 0, str_ts) );
                offset += 8;
            }
            else {
                guint32 event_val = tvb_get_ntohl( tvb, offset );
                guint32 queue_id = (event_val & MASK_QUEUE_ID) >> 27;
                guint32 plen = (event_val & MASK_PACKET_LEN) >> 19;
                guint32 ts_btm_me = (ts_btm & ~MASK_TIME_LSB) | (event_val & MASK_TIME_LSB);
                str_ts = timestamp8ns_to_string( ts_top, ts_btm_me );
                proto_tree_add_string( of_tree, hf_of_short_event, tvb, offset, 8, event_to_string(type, queue_id, plen, str_ts) );
                offset += 4;
            }
        }
    }
}
