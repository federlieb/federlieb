#include "vt_miniucd.hxx"

namespace fl = ::federlieb;

void
vt_miniucd::xConnect(bool create)
{
  declare(R"SQL(
      CREATE TABLE fl_miniucd(

        "cp" INTEGER NOT NULL, "age" TEXT, "na" TEXT, "JSN" TEXT,
        "gc" TEXT, "ccc" TEXT, "dt" TEXT, "dm" TEXT,
        "nt" TEXT, "nv" TEXT, "bc" TEXT, "bpt" TEXT,
        "bpb" TEXT, "Bidi_M" TEXT, "bmg" TEXT, "suc" TEXT,
        "slc" TEXT, "stc" TEXT, "uc" TEXT, "lc" TEXT,
        "tc" TEXT, "scf" TEXT, "cf" TEXT, "jt" TEXT,
        "jg" TEXT, "ea" TEXT, "lb" TEXT, "sc" TEXT,
        "scx" TEXT, "Dash" TEXT, "WSpace" TEXT, "Hyphen" TEXT,
        "QMark" TEXT, "Radical" TEXT, "Ideo" TEXT, "UIdeo" TEXT,
        "IDSB" TEXT, "IDST" TEXT, "hst" TEXT, "DI" TEXT,
        "ODI" TEXT, "Alpha" TEXT, "OAlpha" TEXT, "Upper" TEXT,
        "OUpper" TEXT, "Lower" TEXT, "OLower" TEXT, "Math" TEXT,
        "OMath" TEXT, "Hex" TEXT, "AHex" TEXT, "NChar" TEXT,
        "VS" TEXT, "Bidi_C" TEXT, "Join_C" TEXT, "Gr_Base" TEXT,
        "Gr_Ext" TEXT, "OGr_Ext" TEXT, "Gr_Link" TEXT, "STerm" TEXT,
        "Ext" TEXT, "Term" TEXT, "Dia" TEXT, "Dep" TEXT,
        "IDS" TEXT, "OIDS" TEXT, "XIDS" TEXT, "IDC" TEXT,
        "OIDC" TEXT, "XIDC" TEXT, "SD" TEXT, "LOE" TEXT,
        "Pat_WS" TEXT, "Pat_Syn" TEXT, "GCB" TEXT, "WB" TEXT,
        "SB" TEXT, "CE" TEXT, "Comp_Ex" TEXT, "NFC_QC" TEXT,
        "NFD_QC" TEXT, "NFKC_QC" TEXT, "NFKD_QC" TEXT, "XO_NFC" TEXT,
        "XO_NFD" TEXT, "XO_NFKC" TEXT, "XO_NFKD" TEXT, "FC_NFKC" TEXT,
        "CI" TEXT, "Cased" TEXT, "CWCF" TEXT, "CWCM" TEXT,
        "CWKCF" TEXT, "CWL" TEXT, "CWT" TEXT, "CWU" TEXT,
        "NFKC_CF" TEXT, "InSC" TEXT, "InPC" TEXT, "PCM" TEXT,
        "vo" TEXT, "RI" TEXT, "blk" TEXT, "isc" TEXT,
        "na1" TEXT, "Emoji" TEXT, "EPres" TEXT, "EMod" TEXT,
        "EBase" TEXT, "EComp" TEXT, "ExtPict" TEXT
 
       )
    )SQL");
}

bool
vt_miniucd::xBestIndex(fl::vtab::index_info& info)
{
  return true;
}

fl::stmt
vt_miniucd::xFilter(const fl::vtab::index_info& info, cursor* cursor)
{

  auto stmt = db().prepare(miniucd_sql).execute();

  return stmt;
}
