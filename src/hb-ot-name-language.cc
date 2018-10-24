/*
 * Copyright © 2018  Google, Inc.
 *
 *  This is part of HarfBuzz, a text shaping library.
 *
 * Permission is hereby granted, without written agreement and without
 * license or royalty fees, to use, copy, modify, and distribute this
 * software and its documentation for any purpose, provided that the
 * above copyright notice and the following two paragraphs appear in
 * all copies of this software.
 *
 * IN NO EVENT SHALL THE COPYRIGHT HOLDER BE LIABLE TO ANY PARTY FOR
 * DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES
 * ARISING OUT OF THE USE OF THIS SOFTWARE AND ITS DOCUMENTATION, EVEN
 * IF THE COPYRIGHT HOLDER HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 *
 * THE COPYRIGHT HOLDER SPECIFICALLY DISCLAIMS ANY WARRANTIES, INCLUDING,
 * BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS FOR A PARTICULAR PURPOSE.  THE SOFTWARE PROVIDED HEREUNDER IS
 * ON AN "AS IS" BASIS, AND THE COPYRIGHT HOLDER HAS NO OBLIGATION TO
 * PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR MODIFICATIONS.
 *
 * Google Author(s): Behdad Esfahbod
 */

#include "hb-ot-name-language.hh"

struct hb_ot_language_map_t
{
  static int cmp (const void *key, const void *item)
  {
    unsigned int a = * (unsigned int *) key;
    unsigned int b = ((const hb_ot_language_map_t *) item)->code;
    return a < b ? -1 : a > b ? +1 : 0;
  }

  uint16_t	code;
  char		lang[6];
};

static const hb_ot_language_map_t
hb_ms_language_map[] =
{
  {0x0401,	"ar"}, /* Arabic (Saudi Arabia) */
  {0x0402,	"bg"}, /* Bulgarian (Bulgaria) */
  {0x0404,	"zh-tw"}, /* Chinese (Taiwan) */
  {0x0405,	"cs"}, /* Czech (Czech Republic) */
  {0x0406,	"da"}, /* Danish (Denmark) */
  {0x0407,	"de"}, /* German (Germany) */
  {0x0408,	"el"}, /* Greek (Greece) */
  {0x0409,	"en"}, /* English (United States) */
  {0x040A,	"es"}, /* Spanish (Traditional Sort) (Spain) */
  {0x040B,	"fi"}, /* Finnish (Finland) */
  {0x040C,	"fr"}, /* French (France) */
  {0x040D,	"he"}, /* Hebrew (Israel) */
  {0x040E,	"hu"}, /* Hungarian (Hungary) */
  {0x040F,	"is"}, /* Icelandic (Iceland) */
  {0x0410,	"it"}, /* Italian (Italy) */
  {0x0411,	"ja"}, /* Japanese (Japan) */
  {0x0413,	"nl"}, /* Dutch (Netherlands) */
  {0x0414,	"no"}, /* Norwegian (Bokmal) (Norway) */
  {0x0415,	"pl"}, /* Polish (Poland) */
  {0x0416,	"pt"}, /* Portuguese (Brazil) */
  {0x0418,	"ro"}, /* Romanian (Romania) */
  {0x0419,	"ru"}, /* Russian (Russia) */
  {0x041A,	"hr"}, /* Croatian (Croatia) */
  {0x041B,	"sk"}, /* Slovak (Slovakia) */
  {0x041C,	"sq"}, /* Albanian (Albania) */
  {0x041D,	"sv"}, /* Swedish (Sweden) */
  {0x041E,	"th"}, /* Thai (Thailand) */
  {0x041F,	"tr"}, /* Turkish (Turkey) */
  {0x0420,	"ur"}, /* Urdu (Islamic Republic of Pakistan) */
  {0x0421,	"id"}, /* Indonesian (Indonesia) */
  {0x0422,	"uk"}, /* Ukrainian (Ukraine) */
  {0x0423,	"be"}, /* Belarusian (Belarus) */
  {0x0425,	"et"}, /* Estonian (Estonia) */
  {0x0426,	"lv"}, /* Latvian (Latvia) */
  {0x0427,	"lt"}, /* Lithuanian (Lithuania) */
  {0x0428,	"tg"}, /* Tajik (Cyrillic) (Tajikistan) */
  {0x042A,	"vi"}, /* Vietnamese (Vietnam) */
  {0x042B,	"hy"}, /* Armenian (Armenia) */
  {0x042C,	"az"}, /* Azeri (Latin) (Azerbaijan) */
  {0x042F,	"mk"}, /* Macedonian (FYROM) (Former Yugoslav Republic of Macedonia) */
  {0x0436,	"af"}, /* Afrikaans (South Africa) */
  {0x0437,	"ka"}, /* Georgian (Georgia) */
  {0x0438,	"fo"}, /* Faroese (Faroe Islands) */
  {0x0439,	"hi"}, /* Hindi (India) */
  {0x043A,	"mt"}, /* Maltese (Malta) */
  {0x043E,	"ms"}, /* Malay (Malaysia) */
  {0x0442,	"tk"}, /* Turkmen (Turkmenistan) */
  {0x0443,	"uz"}, /* Uzbek (Latin) (Uzbekistan) */
  {0x0445,	"bn"}, /* Bengali (India) */
  {0x0446,	"pa"}, /* Punjabi (India) */
  {0x0447,	"gu"}, /* Gujarati (India) */
  {0x0449,	"ta"}, /* Tamil (India) */
  {0x044A,	"te"}, /* Telugu (India) */
  {0x044B,	"kn"}, /* Kannada (India) */
  {0x044C,	"ml"}, /* Malayalam (India) */
  {0x044D,	"as"}, /* Assamese (India) */
  {0x044E,	"mr"}, /* Marathi (India) */
  {0x044F,	"sa"}, /* Sanskrit (India) */
  {0x0450,	"mn"}, /* Mongolian (Cyrillic) (Mongolia) */
  {0x0453,	"km"}, /* Khmer (Cambodia) */
  {0x0454,	"lo"}, /* Lao (Lao P.D.R.) */
  {0x0457,	"kok"}, /* Konkani (India) */
  {0x045A,	"syr"}, /* Syriac (Syria) */
  {0x045D,	"iu"}, /* Inuktitut (Canada) */
  {0x045E,	"am"}, /* Amharic (Ethiopia) */
  {0x0461,	"ne"}, /* Nepali (Nepal) */
  {0x0462,	"fy"}, /* Frisian (Netherlands) */
  {0x0463,	"ps"}, /* Pashto (Afghanistan) */
  {0x0464,	"phi"}, /* Filipino (Philippines) */
  {0x0465,	"div"}, /* Divehi (Maldives) */
  {0x0468,	"ha"}, /* Hausa (Latin) (Nigeria) */
  {0x046A,	"yo"}, /* Yoruba (Nigeria) */
  {0x0470,	"ibo"}, /* Igbo (Nigeria) */
  {0x0481,	"mi"}, /* Maori (New Zealand) */
  {0x0801,	"ar"}, /* Arabic (Iraq) */
  {0x0804,	"zh-cn"}, /* Chinese (People’s Republic of China) */
  {0x0807,	"de"}, /* German (Switzerland) */
  {0x0809,	"en"}, /* English (United Kingdom) */
  {0x080A,	"es"}, /* Spanish (Mexico) */
  {0x080C,	"fr"}, /* French (Belgium) */
  {0x0810,	"it"}, /* Italian (Switzerland) */
  {0x0813,	"nl"}, /* Dutch (Belgium) */
  {0x0814,	"nn"}, /* Norwegian (Nynorsk) (Norway) */
  {0x0816,	"pt"}, /* Portuguese (Portugal) */
  {0x081A,	"sr"}, /* Serbian (Latin) (Serbia) */
  {0x081D,	"sv"}, /* Sweden (Finland) */
  {0x082C,	"az"}, /* Azeri (Cyrillic) (Azerbaijan) */
  {0x083E,	"ms"}, /* Malay (Brunei Darussalam) */
  {0x0843,	"uz"}, /* Uzbek (Cyrillic) (Uzbekistan) */
  {0x0845,	"bn"}, /* Bengali (Bangladesh) */
  {0x0C01,	"ar"}, /* Arabic (Egypt) */
  {0x0C04,	"zh-hk"}, /* Chinese (Hong Kong S.A.R.) */
  {0x0C07,	"de"}, /* German (Austria) */
  {0x0C09,	"en"}, /* English (Australia) */
  {0x0C0C,	"fr"}, /* French (Canada) */
  {0x0C1A,	"sr"}, /* Serbian (Cyrillic) (Serbia) */
  {0x1001,	"ar"}, /* Arabic (Libya) */
  {0x1004,	"zh-sg"}, /* Chinese (Singapore) */
  {0x1007,	"de"}, /* German (Luxembourg) */
  {0x1009,	"en"}, /* English (Canada) */
  {0x100A,	"es"}, /* Spanish (Guatemala) */
  {0x100C,	"fr"}, /* French (Switzerland) */
  {0x1401,	"ar"}, /* Arabic (Algeria) */
  {0x1409,	"en"}, /* English (New Zealand) */
  {0x140A,	"es"}, /* Spanish (Costa Rica) */
  {0x141A,	"bs"}, /* Bosnian (Latin) (Bosnia and Herzegovina) */
  {0x1801,	"ar"}, /* Arabic (Morocco) */
  {0x1809,	"en"}, /* English (Ireland) */
  {0x180A,	"es"}, /* Spanish (Panama) */
  {0x180C,	"fr"}, /* French (Principality of Monaco) */
  {0x1C01,	"ar"}, /* Arabic (Tunisia) */
  {0x1C09,	"en"}, /* English (South Africa) */
  {0x1C0A,	"es"}, /* Spanish (Dominican Republic) */
  {0x2001,	"ar"}, /* Arabic (Oman) */
  {0x2009,	"en"}, /* English (Jamaica) */
  {0x200A,	"es"}, /* Spanish (Venezuela) */
  {0x2401,	"ar"}, /* Arabic (Yemen) */
  {0x2409,	"en"}, /* English (Caribbean) */
  {0x240A,	"es"}, /* Spanish (Colombia) */
  {0x2801,	"ar"}, /* Arabic (Syria) */
  {0x2809,	"en"}, /* English (Belize) */
  {0x280A,	"es"}, /* Spanish (Peru) */
  {0x2C01,	"ar"}, /* Arabic (Jordan) */
  {0x2C09,	"en"}, /* English (Trinidad and Tobago) */
  {0x2C0A,	"es"}, /* Spanish (Argentina) */
  {0x3001,	"ar"}, /* Arabic (Lebanon) */
  {0x3009,	"en"}, /* English (Zimbabwe) */
  {0x300A,	"es"}, /* Spanish (Ecuador) */
  {0x3401,	"ar"}, /* Arabic (Kuwait) */
  {0x3409,	"en"}, /* English (Republic of the Philippines) */
  {0x340A,	"es"}, /* Spanish (Chile) */
  {0x3801,	"ar"}, /* Arabic (U.A.E.) */
  {0x380A,	"es"}, /* Spanish (Uruguay) */
  {0x3C01,	"ar"}, /* Arabic (Bahrain) */
  {0x3C0A,	"es"}, /* Spanish (Paraguay) */
  {0x4001,	"ar"}, /* Arabic (Qatar) */
  {0x4009,	"en"}, /* English (India) */
  {0x400A,	"es"}, /* Spanish (Bolivia) */
  {0x4409,	"en"}, /* English (Malaysia) */
  {0x440A,	"es"}, /* Spanish (El Salvador) */
  {0x4809,	"en"}, /* English (Singapore) */
  {0x480A,	"es"}, /* Spanish (Honduras) */
  {0x4C0A,	"es"}, /* Spanish (Nicaragua) */
  {0x500A,	"es"}, /* Spanish (Puerto Rico) */
  {0x540A,	"es"}, /* Spanish (United States) */
};

static const hb_ot_language_map_t
hb_mac_language_map[] =
{
  {0,	"en"}, /* English */
  {1,	"fr"}, /* French */
  {2,	"de"}, /* German */
  {3,	"it"}, /* Italian */
  {4,	"nl"}, /* Dutch */
  {5,	"sv"}, /* Swedish */
  {6,	"es"}, /* Spanish */
  {7,	"da"}, /* Danish */
  {8,	"pt"}, /* Portuguese */
  {9,	"no"}, /* Norwegian */
  {10,	"he"}, /* Hebrew */
  {11,	"ja"}, /* Japanese */
  {12,	"ar"}, /* Arabic */
  {13,	"fi"}, /* Finnish */
  {14,	"el"}, /* Greek */
  {15,	"is"}, /* Icelandic */
  {16,	"mt"}, /* Maltese */
  {17,	"tr"}, /* Turkish */
  {18,	"hr"}, /* Croatian */
  {19,	"zh-tw"}, /* Chinese (Traditional) */
  {20,	"ur"}, /* Urdu */
  {21,	"hi"}, /* Hindi */
  {22,	"th"}, /* Thai */
  {23,	"ko"}, /* Korean */
  {24,	"lt"}, /* Lithuanian */
  {25,	"pl"}, /* Polish */
  {26,	"hu"}, /* Hungarian */
  {27,	"et"}, /* Estonian */
  {28,	"lv"}, /* Latvian */
  {30,	"fo"}, /* Faroese */
  {31,	"fa"}, /* Farsi/Persian */
  {32,	"ru"}, /* Russian */
  {33,	"zh-cn"}, /* Chinese (Simplified) */
  {34,	"nl"}, /* Flemish */
  {35,	"ga"}, /* Irish Gaelic */
  {36,	"sq"}, /* Albanian */
  {37,	"ro"}, /* Romanian */
  {38,	"cs"}, /* Czech */
  {39,	"sk"}, /* Slovak */
  {40,	"sl"}, /* Slovenian */
  {41,	"yi"}, /* Yiddish */
  {42,	"sr"}, /* Serbian */
  {43,	"mk"}, /* Macedonian */
  {44,	"bg"}, /* Bulgarian */
  {45,	"uk"}, /* Ukrainian */
  {46,	"be"}, /* Byelorussian */
  {47,	"uz"}, /* Uzbek */
  {48,	"kk"}, /* Kazakh */
  {49,	"az"}, /* Azerbaijani (Cyrillic script) */
  {49,	"az"}, /* Azerbaijani (Cyrillic script) */
  {50,	"ar"}, /* Azerbaijani (Arabic script) */
  {51,	"hy"}, /* Armenian */
  {52,	"ka"}, /* Georgian */
  {53,	"mo"}, /* Moldavian */
  {54,	"ky"}, /* Kirghiz */
  {55,	"tg"}, /* Tajiki */
  {56,	"tk"}, /* Turkmen */
  {57,	"mo"}, /* Mongolian (Mongolian script) */
  {57,	"mo"}, /* Mongolian (Mongolian script) */
  {58,	"mo"}, /* Mongolian (Cyrillic script) */
  {59,	"ps"}, /* Pashto */
  {60,	"ku"}, /* Kurdish */
  {61,	"ks"}, /* Kashmiri */
  {62,	"sd"}, /* Sindhi */
  {63,	"bo"}, /* Tibetan */
  {64,	"ne"}, /* Nepali */
  {65,	"sa"}, /* Sanskrit */
  {66,	"mr"}, /* Marathi */
  {67,	"bn"}, /* Bengali */
  {68,	"as"}, /* Assamese */
  {69,	"gu"}, /* Gujarati */
  {70,	"pa"}, /* Punjabi */
  {71,	"or"}, /* Oriya */
  {72,	"ml"}, /* Malayalam */
  {73,	"kn"}, /* Kannada */
  {74,	"ta"}, /* Tamil */
  {75,	"te"}, /* Telugu */
  {76,	"si"}, /* Sinhalese */
  {77,	"my"}, /* Burmese */
  {78,	"km"}, /* Khmer */
  {79,	"lo"}, /* Lao */
  {80,	"vi"}, /* Vietnamese */
  {81,	"id"}, /* Indonesian */
  {82,	"tl"}, /* Tagalog */
  {83,	"ms"}, /* Malay (Roman script) */
  {84,	"ms"}, /* Malay (Arabic script) */
  {85,	"am"}, /* Amharic */
  {86,	"ti"}, /* Tigrinya */
  {87,	"om"}, /* Galla */
  {88,	"so"}, /* Somali */
  {89,	"sw"}, /* Swahili */
  {90,	"rw"}, /* Kinyarwanda/Ruanda */
  {91,	"rn"}, /* Rundi */
  {92,	"ny"}, /* Nyanja/Chewa */
  {93,	"mg"}, /* Malagasy */
  {94,	"eo"}, /* Esperanto */
  {128,	"cy"}, /* Welsh */
  {129,	"eu"}, /* Basque */
  {130,	"ca"}, /* Catalan */
  {131,	"la"}, /* Latin */
  {132,	"qu"}, /* Quechua */
  {133,	"gn"}, /* Guarani */
  {134,	"ay"}, /* Aymara */
  {135,	"tt"}, /* Tatar */
  {136,	"ug"}, /* Uighur */
  {137,	"dz"}, /* Dzongkha */
  {138,	"jw"}, /* Javanese (Roman script) */
  {139,	"su"}, /* Sundanese (Roman script) */
  {140,	"gl"}, /* Galician */
  {141,	"af"}, /* Afrikaans */
  {142,	"br"}, /* Breton */
  {143,	"iu"}, /* Inuktitut */
  {144,	"gd"}, /* Scottish Gaelic */
  {145,	"gv"}, /* Manx Gaelic */
  {146,	"ga"}, /* Irish Gaelic (with dot above) */
  {147,	"to"}, /* Tongan */
  {148,	"el"}, /* Greek (polytonic) */
  {149,	"ik"}, /* Greenlandic */
  {150,	"az"}, /* Azerbaijani (Roman script) */
};


static hb_language_t
_hb_ot_name_language_for (unsigned int code,
			  const hb_ot_language_map_t *array,
			  unsigned int len)
{
  const hb_ot_language_map_t *entry = (const hb_ot_language_map_t *)
				      hb_bsearch (&code,
						  array,
						  len,
						  sizeof (array[0]),
						  hb_ot_language_map_t::cmp);

  if (entry)
    return hb_language_from_string (entry->lang, -1);

  return HB_LANGUAGE_INVALID;
}

hb_language_t
_hb_ot_name_language_for_ms_code (unsigned int code)
{
  return _hb_ot_name_language_for (code,
				   hb_ms_language_map,
				   ARRAY_LENGTH (hb_ms_language_map));
}

hb_language_t
_hb_ot_name_language_for_mac_code (unsigned int code)
{
  return _hb_ot_name_language_for (code,
				   hb_mac_language_map,
				   ARRAY_LENGTH (hb_mac_language_map));
}
