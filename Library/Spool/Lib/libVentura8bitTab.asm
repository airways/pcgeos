
COMMENT @%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

	Copyright (c) GeoWorks 1990 -- All Rights Reserved

PROJECT:	PC GEOS
MODULE:		Spool
FILE:		libVentura8bitTab.asm

DESCRIPTION:
	conversion table for upper 128 characters of the GEOS symbol set to
	Ventura symbol set.
		

	$Id: libVentura8bitTab.asm,v 1.1 97/04/07 11:10:50 newdeal Exp $

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%@

VenturaTable	segment	resource

if	_TEXT_PRINTING
;VenturaTab:
	byte	216	;C_UA_DIERESIS	, 0x80
	byte	208	;C_UA_RING	, 0x81
	byte	180	;C_UC_CEDILLA	, 0x82
	byte	220	;C_UE_ACUTE	, 0x83
	byte	182	;C_UN_TILDE	, 0x84
	byte	218	;C_UO_DIERESIS	, 0x85
	byte	219	;C_UU_DIERESIS	, 0x86
	byte	196	;C_LA_ACUTE	, 0x87
	byte	200	;C_LA_GRAVE	, 0x88
	byte	192	;C_LA_CIRCUMFLEX, 0x89
	byte	204	;C_LA_DIERESIS	, 0x8a
	byte	226	;C_LA_TILDE	, 0x8b
	byte	212	;C_LA_RING	, 0x8c
	byte	181	;C_LC_CEDILLA	, 0x8d
	byte	197	;C_LE_ACUTE	, 0x8e
	byte	201	;C_LE_GRAVE	, 0x8f
	byte	193	;C_LE_CIRCUMFLEX, 0x90
	byte	205	;C_LE_DIERESIS	, 0x91
	byte	213	;C_LI_ACUTE	, 0x92
	byte	217	;C_LI_GRAVE	, 0x93
	byte	209	;C_LI_CIRCUMFLEX, 0x94
	byte	221	;C_LI_DIERESIS	, 0x95
	byte	183	;C_LN_TILDE	, 0x96
	byte	198	;C_LO_ACUTE	, 0x97
	byte	202	;C_LO_GRAVE	, 0x98
	byte	194	;C_LO_CIRCUMFLEX, 0x99
	byte	206	;C_LO_DIERESIS	, 0x9a
	byte	234	;C_LO_TILDE	, 0x9b
	byte	199	;C_LU_ACUTE	, 0x9c
	byte	203	;C_LU_GRAVE	, 0x9d
	byte	195	;C_LU_CIRCUMFLEX, 0x9e
	byte	207	;C_LU_DIERESIS	, 0x9f
	byte	243	;C_DAGGER	, 0xa0
	byte	179	;C_DEGREE	, 0xa1
	byte	191	;C_CENT		, 0xa2
	byte	187	;C_STERLING	, 0xa3
	byte	189	;C_SECTION	, 0xa4
	byte	252	;C_BULLET	, 0xa5
	byte	242	;C_PARAGRAPH	, 0xa6
	byte	222	;C_GERMANDBLS	, 0xa7
	byte	169	;C_REGISTERED	, 0xa8
	byte	168	;C_COPYRIGHT	, 0xa9
	byte	170	;C_TRADEMARK	, 0xaa
	byte	C_SPACE	;C_ACUTE	, 0xab
	byte	C_SPACE	;C_DIERESIS	, 0xac
	byte	C_SPACE	;C_NOTEQUAL	, 0xad
	byte	211	;C_U_AE		, 0xae
	byte	210	;C_UO_SLASH	, 0xaf
	byte	C_SPACE	;C_INFINITY	, 0xb0
	byte	C_SPACE	;C_PLUSMINUS	, 0xb1
	byte	C_SPACE	;C_LESSEQUAL	, 0xb2
	byte	C_SPACE	;C_GREATEREQUAL	, 0xb3
	byte	188	;C_YEN		, 0xb4
	byte	C_SPACE	;C_L_MU		, 0xb5
	byte	C_SPACE	;C_L_DELTA	, 0xb6
	byte	C_SPACE	;C_U_SIGMA	, 0xb7
	byte	C_SPACE	;C_U_PI		, 0xb8
	byte	C_SPACE	;C_L_PI		, 0xb9
	byte	C_SPACE	;C_INTEGRAL	, 0xba
	byte	249	;C_ORDFEMININE	, 0xbb
	byte	250	;C_ORDMASCULINE	, 0xbc
	byte	C_SPACE	;C_U_OMEGA	, 0xbd
	byte	215	;C_L_AE		, 0xbe
	byte	214	;C_LO_SLASH	, 0xbf
	byte	185	;C_QUESTIONDOWN	, 0xc0
	byte	184	;C_EXCLAMDOWN	, 0xc1
	byte	C_SPACE	;C_LOGICAL_NOT	, 0xc2
	byte	C_SPACE	;C_ROOT		, 0xc3
	byte	190	;C_FLORIN	, 0xc4
	byte	C_SPACE	;C_APPROX_EQUAL	, 0xc5
	byte	C_SPACE	;C_U_DELTA	, 0xc6
	byte	251	;C_GUILLEDBLLEFT, 0xc7
	byte	253	;C_GUILLEDBLRIGHT, 0xc8
	byte	255	;C_ELLIPSIS	, 0xc9
	byte	C_SPACE	;C_NONBRKSPACE	, 0xca
	byte	161	;C_UA_GRAVE	, 0xcb
	byte	225	;C_UA_TILDE	, 0xcc
	byte	233	;C_UO_TILDE	, 0xcd
	byte	240	;C_U_OE		, 0xce
	byte	241	;C_L_OE		, 0xcf
	byte	246	;C_ENDASH	, 0xd0
	byte	245	;C_EMDASH	, 0xd1
	byte	177	;C_QUOTEDBLLEFT	, 0xd2
	byte	160	;C_QUOTEDBLRIGHT, 0xd3
	byte	0x27	;C_QUOTESNGLEFT	, 0xd4
	byte	0x27	;C_QUOTESNGRIGHT, 0xd5
	byte	C_SPACE	;C_DIVISION	, 0xd6
	byte	C_SPACE	;C_DIAMONDBULLET, 0xd7
	byte	239	;C_LY_DIERESIS	, 0xd8
	byte	238	;C_UY_DIERESIS	, 0xd9
	byte	47	;C_FRACTION	, 0xda
	byte	186	;C_CURRENCY	, 0xdb
	byte	171	;C_GUILSNGLEFT	, 0xdc
	byte	172	;C_GUILSNGRIGHT	, 0xdd
	byte	C_SPACE	;C_LY_ACUTE	, 0xde
	byte	C_SPACE	;C_UY_ACUTE	, 0xdf
	byte	244	;C_DBLDAGGER	, 0xe0
	byte	C_SPACE	;C_CNTR_DOT	, 0xe1
	byte	0x2c	;C_SNGQUOTELOW	, 0xe2
	byte	C_SPACE	;C_DBLQUOTELOW	, 0xe3
	byte	176	;C_PERTHOUSAND	, 0xe4
	byte	162	;C_UA_CIRCUMFLEX, 0xe5
	byte	164	;C_UE_CIRCUMFLEX, 0xe6
	byte	224	;C_UA_ACUTE	, 0xe7
	byte	165	;C_UE_DIERESIS	, 0xe8
	byte	163	;C_UE_GRAVE	, 0xe9
	byte	229	;C_UI_ACUTE	, 0xea
	byte	166	;C_UI_CIRCUMFLEX, 0xeb
	byte	167	;C_UI_DIERESIS	, 0xec
	byte	230	;C_UI_GRAVE	, 0xed
	byte	231	;C_UO_ACUTE	, 0xee
	byte	223	;C_UO_CIRCUMFLEX, 0xef
	byte	C_SPACE	;C_LOGO		, 0xf0
	byte	232	;C_UO_GRAVE	, 0xf1
	byte	C_SPACE	;C_UU_ACUTE	, 0xf2
	byte	174	;C_UU_CIRCUMFLEX, 0xf3
	byte	173	;C_UU_GRAVE	, 0xf4
	byte	C_SPACE	;C_LI_DOTLESS	, 0xf5
	byte	94	;C_CIRCUMFLEX	, 0xf6
	byte	126	;C_TILDE	, 0xf7
	byte	C_SPACE	;C_MACRON	, 0xf8
	byte	C_SPACE	;C_BREVE	, 0xf9
	byte	C_SPACE	;C_DOTACCENT	, 0xfa
	byte	179	;C_RING		, 0xfb
	byte	C_SPACE	;C_CEDILLA	, 0xfc
	byte	C_SPACE	;C_HUNGARUMLAT	, 0xfd
	byte	C_SPACE	;C_OGONEK	, 0xfe
	byte	C_SPACE	;C_CARON	, 0xff
endif	;_TEXT_PRINTING

VenturaTable	ends
