#pragma once

#include "str.h"
#include "arena.h"

typedef struct xmlattr_t {
    strview_t key;
    strview_t value;
    struct xmlattr_t *next;
} xmlattr_t;

typedef struct xmltag_t {
    strview_t key;
    xmlattr_t *attributes;
    strview_t content;
    struct xmltag_t *child;
    struct xmltag_t *tail;
    struct xmltag_t *next;
} xmltag_t;

typedef struct {
    str_t text;
    xmltag_t *root;
    xmltag_t *tail;
} xml_t;

xml_t xmlParse(arena_t *arena, strview_t filename);
xml_t xmlParseStr(arena_t *arena, str_t xmlstr);

xmltag_t *xmlGetTag(xmltag_t *parent, strview_t key, bool recursive);
strview_t xmlGetAttribute(xmltag_t *tag, strview_t key);