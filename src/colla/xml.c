#include "xml.h"

#include "file.h"
#include "strstream.h"
#include "tracelog.h"

static xmltag_t *xml__parse_tag(arena_t *arena, instream_t *in);

xml_t xmlParse(arena_t *arena, strview_t filename) {
    return xmlParseStr(arena, fileReadWholeStr(arena, filename));
}

xml_t xmlParseStr(arena_t *arena, str_t xmlstr) {
    xml_t out = {
        .text = xmlstr,
        .root = alloc(arena, xmltag_t),
    };

    

    instream_t in = istrInitLen(xmlstr.buf, xmlstr.len);

    while (!istrIsFinished(in)) {
        xmltag_t *tag = xml__parse_tag(arena, &in);

        if (out.tail) out.tail->next = tag;
        else          out.root->child = tag;

        out.tail = tag;
    }

    return out;
}

xmltag_t *xmlGetTag(xmltag_t *parent, strview_t key, bool recursive) {
    xmltag_t *t = parent ? parent->child : NULL;
    while (t) {
        if (strvEquals(key, t->key)) {
            return t;
        }
        if (recursive && t->child) {
            xmltag_t *out = xmlGetTag(t, key, recursive);
            if (out) {
                return out;
            }
        }
        t = t->next;
    }
    return NULL;
}

strview_t xmlGetAttribute(xmltag_t *tag, strview_t key) {
    xmlattr_t *a = tag ? tag->attributes : NULL;
    while (a) {
        if (strvEquals(key, a->key)) {
            return a->value;
        }
        a = a->next;
    }
    return (strview_t){0};
}

// == PRIVATE FUNCTIONS ========================================================================

static xmlattr_t *xml__parse_attr(arena_t *arena, instream_t *in) {
    if (istrPeek(in) != ' ') {
        return NULL;
    }

    strview_t key = strvTrim(istrGetView(in, '='));
    istrSkip(in, 2); // skip = and "
    strview_t val = strvTrim(istrGetView(in, '"'));
    istrSkip(in, 1); // skip "

    if (strvIsEmpty(key) || strvIsEmpty(val)) {
        warn("key or value empty");
        return NULL;
    }
    
    xmlattr_t *attr = alloc(arena, xmlattr_t);
    attr->key = key;
    attr->value = val;
    return attr;
}

static xmltag_t *xml__parse_tag(arena_t *arena, instream_t *in) {
    istrSkipWhitespace(in);

    // we're either parsing the body, or we have finished the object
    if (istrPeek(in) != '<' || istrPeekNext(in) == '/') {
        return NULL;
    }

    istrSkip(in, 1); // skip <

    // meta tag, we don't care about these
    if (istrPeek(in) == '?') {
        istrIgnoreAndSkip(in, '\n');
        return NULL;
    }

    xmltag_t *tag = alloc(arena, xmltag_t);

    tag->key = strvTrim(istrGetViewEither(in, strv(" >")));

    xmlattr_t *attr = xml__parse_attr(arena, in);
    while (attr) {
        attr->next = tag->attributes;
        tag->attributes = attr;
        attr = xml__parse_attr(arena, in);
    }

    // this tag does not have children, return
    if (istrPeek(in) == '/') {
        istrSkip(in, 2); // skip / and >
        return tag;
    }

    istrSkip(in, 1); // skip >

    xmltag_t *child = xml__parse_tag(arena, in);
    while (child) {
        if (tag->tail) {
            tag->tail->next = child;
            tag->tail = child;
        }
        else {
            tag->child = tag->tail = child;
        }
        child = xml__parse_tag(arena, in);
    }

    // parse content
    istrSkipWhitespace(in);
    tag->content = istrGetView(in, '<');

    // closing tag
    istrSkip(in, 2); // skip < and /
    strview_t closing = strvTrim(istrGetView(in, '>'));
    if (!strvEquals(tag->key, closing)) {
        warn("opening and closing tags are different!: (%v) != (%v)", tag->key, closing);
    }
    istrSkip(in, 1); // skip >
    return tag;
}
