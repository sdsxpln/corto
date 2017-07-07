/* Copyright (c) 2010-2017 the corto developers
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "corto/corto.h"
#include "_object.h"

/*#define CORTO_WALK_TRACING*/
#ifdef CORTO_WALK_TRACING
static int indent = 0;
#endif

int16_t corto_any_walk(corto_walk_opt* this, corto_value* info, void* userData);

int16_t corto_walk_ptr(corto_walk_opt* this, void *ptr, corto_type type, void* userData) {
    corto_value v = corto_value_value(ptr, type);
    return corto_walk_value(this, &v, userData);
}

int16_t corto_walk_observable(corto_walk_opt* this, corto_value* info, void* userData) {
    corto_walk_cb cb = this->metaprogram[CORTO_MEMBER];
    if (!cb) {
        cb = corto_walk_value;
    }

    corto_value_ptrset(info, *(void**)corto_value_ptrof(info));

    if (cb) {
        return cb(this, info, userData);
    } else {
        return 0;
    }
}

/* Forward value to the right callback function */
int16_t corto_walk_value(corto_walk_opt* this, corto_value* info, void* userData) {
    corto_type t;
    int16_t result;
    corto_walk_cb cb;
    bool isObservable = false;

    if (info->kind == CORTO_MEMBER) {
        corto_member m = info->is.member.t;
        if (m->modifiers & CORTO_OBSERVABLE) {
            isObservable = true;
        }
    }

    t = corto_value_typeof(info);

    cb = NULL;

    if (!this->initialized) {
        corto_assert(0, "serializer is not initialized!");
    }

    if (!this->constructed) {
        if (this->construct) {
            if (this->construct(this, info, userData)) {
                goto error;
            }
        }
        this->constructed = true;
    }

    /* If the serializer has a special handler for reference types, use it in case the
     * type is a reference type. */
    if (t->reference && ((t->kind == CORTO_VOID) || 
                         ((info->kind != CORTO_OBJECT) && 
                          (info->kind != CORTO_BASE) && 
                          (info->kind != CORTO_MEM) &&
                          !isObservable)))
    {
        cb = this->reference;
    } else
    /* ..otherwise use the program-handler */
    if (!cb) {
        cb = this->program[t->kind];
    }

    result = 0;
    if (cb) {
        result = cb(this, info, userData);
    }

    return result;
error:
    return -1;
}

void corto_walk_init(corto_walk_opt* this) {
    memset(this, 0, sizeof(corto_walk_opt));
    this->program[CORTO_ANY] = corto_any_walk;
    this->program[CORTO_COMPOSITE] = corto_walk_members;
    this->program[CORTO_COLLECTION] = corto_walk_elements;
    this->metaprogram[CORTO_BASE] = corto_walk_value;
    this->observable = corto_walk_observable;
    this->initialized = TRUE;
    this->constructed = FALSE;
    this->access = CORTO_GLOBAL;
    this->accessKind = CORTO_XOR;
    this->aliasAction = CORTO_WALK_ALIAS_FOLLOW;
    this->optionalAction = CORTO_WALK_OPTIONAL_IF_SET;
    this->visitAllCases = FALSE;
}

/* Start serializing */
int16_t corto_walk(corto_walk_opt* this, corto_object o, void* userData) {
    corto_value info;
    corto_walk_cb cb;
    int16_t result;

    if (this->initialized != TRUE) {
        corto_assert(0, "serializer is not initialized!");
    }

    info.kind = CORTO_OBJECT;
    info.parent = NULL;
    info.is.object.o = o;
    info.is.object.t = corto_typeof(o);

    if (this->construct) {
        if (this->construct(this, &info, userData)) {
            goto error;
        }
    }

    this->constructed = TRUE;

    if (!(cb = this->metaprogram[CORTO_OBJECT])) {
        cb = corto_walk_value;
    }

#ifdef CORTO_WALK_TRACING
    {
        corto_id id, id2;
        printf("%*sserialize(%s : %s // %s)\n",
               indent, " ", corto_fullname(o, id), corto_fullname(corto_typeof(o), id2), corto_checkState(o, CORTO_DESTRUCTED)?"destructed":"valid"); fflush(stdout);
        indent++;
    }
#endif

    result = cb(this, &info, userData);

#ifdef CORTO_WALK_TRACING
    indent--;
#endif

    return result;
error:
    return -1;
}

/* Destruct serializerdata */
int16_t corto_walk_deinit(corto_walk_opt* this, void* userData) {
    if (this->destruct) {
        if (this->destruct(this, userData)) {
            goto error;
        }
    }
error:
    return -1;
}

corto_bool corto_serializeMatchAccess(corto_operatorKind accessKind, corto_modifier sa, corto_modifier a) {
    corto_bool result;

    switch(accessKind) {
    case CORTO_OR:
        result = (sa & a);
        break;
    case CORTO_XOR:
        result = (sa & a) == sa;
        break;
    case CORTO_NOT:
        result = !(sa & a);
        break;
    default:
        corto_error("unsupported operator %s for serializer accessKind.", corto_idof(corto_enum_constant(corto_operatorKind_o, accessKind)));
        result = FALSE;
        break;
    }

    return result;
}

/* Serialize any-value */
int16_t corto_any_walk(corto_walk_opt* this, corto_value* info, void* userData) {
    corto_value v;
    corto_any *any;
    int16_t result = 0;

    any = corto_value_ptrof(info);

    if (any->type) {
        v.parent = info;
        v = corto_value_value(any->value, corto_type(any->type));
        result = corto_walk_value(this, &v, userData);
    }

    return result;
}

/* Serialize a single member */
static int16_t corto_walk_member(
    corto_walk_opt* this,
    corto_member m,
    corto_object o,
    void *v,
    corto_walk_cb cb,
    corto_value *info,
    void *userData)
{
    corto_value member;
    corto_modifier modifiers = m->modifiers;
    corto_bool isAlias = corto_instanceof(corto_alias_o, m);
    corto_bool isKey = modifiers & CORTO_KEY;

    if (isKey && this->keyAction == CORTO_WALK_KEY_DATA_ONLY) {
        return 0;
    }

    if (!isKey && this->keyAction == CORTO_WALK_KEY_KEYS_ONLY) {
        return 0;
    }

    if (isAlias && (this->aliasAction == CORTO_WALK_ALIAS_FOLLOW)) {
        do {
            m = corto_alias(m)->member;
        } while (corto_typeof(m) == (corto_type)corto_alias_o);
    }

    if (modifiers & CORTO_OBSERVABLE) {
        cb = this->observable;
    }

    if (!isAlias || (this->aliasAction != CORTO_WALK_ALIAS_IGNORE)) {
        if (corto_serializeMatchAccess(this->accessKind, this->access, modifiers)) {
            member.kind = CORTO_MEMBER;
            member.parent = info;
            member.is.member.o = o;
            member.is.member.t = m;
            if (modifiers & CORTO_OPTIONAL && (this->optionalAction != CORTO_WALK_OPTIONAL_PASSTHROUGH)) {
                member.is.member.v = *(void**)CORTO_OFFSET(v, m->offset);
            } else {
                member.is.member.v = CORTO_OFFSET(v, m->offset);
            }
#ifdef CORTO_WALK_TRACING
            {
                corto_id id, id2;
                printf("%*smember(%s : %s)\n", indent, " ",
                    corto_fullname(m, id2),
                    corto_fullname(member.is.member.t->type, id));
                fflush(stdout);
            }
            indent++;
#endif
            /* Don't serialize if member is optional and not set */
            if (!(modifiers & CORTO_OPTIONAL) ||
                (this->optionalAction == CORTO_WALK_OPTIONAL_ALWAYS) ||
                (this->optionalAction == CORTO_WALK_OPTIONAL_PASSTHROUGH) ||
                member.is.member.v)
            {
                if (cb(this, &member, userData)) {
                    goto error;
                }
            }
#ifdef CORTO_WALK_TRACING
            indent--;
#endif
        }
    }

    return 0;
error:
    return -1;
}

/* Serialize members */
int16_t corto_walk_members(corto_walk_opt* this, corto_value* info, void* userData) {
    corto_interface t;
    corto_void* v;
    corto_uint32 i;
    corto_member m;
    corto_walk_cb cb;
    corto_object o;

    t = corto_interface(corto_value_typeof(info));
    v = corto_value_ptrof(info);
    o = corto_value_objectof(info);

    /* Process inheritance */
    if (!this->members.length) {
        if (corto_class_instanceof(corto_struct_o, t) &&
            corto_serializeMatchAccess(this->accessKind, this->access, corto_struct(t)->baseAccess))
        {
            corto_value base;

            cb = this->metaprogram[CORTO_BASE];

            if (cb && corto_interface(t)->base) {
                base.kind = CORTO_BASE;
                base.parent = info;
                base.is.base.v = v;
                base.is.base.t = corto_type(corto_interface(t)->base);
                base.is.base.o = o;
    #ifdef CORTO_WALK_TRACING
                {
                    corto_id id;
                    printf("%*sbase(%s)\n", indent, " ", corto_fullname(base.is.base.t, id)); fflush(stdout);
                }
                indent++;
    #endif
                if (cb(this, &base, userData)) {
                    goto error;
                }
    #ifdef CORTO_WALK_TRACING
                indent--;
    #endif
            }
        }
    }

    cb = this->metaprogram[CORTO_MEMBER];
    if (!cb) {
        cb = corto_walk_value;
    }

    /* Process members */
    if (this->members.length) {
        for (i = 0; i < this->members.length; i++) {
            m = this->members.buffer[i];
            if (corto_walk_member(this, m, o, v, cb, info, userData)) {
                goto error;
            }
        }
    } else if (!this->visitAllCases && (corto_typeof(t) == corto_type(corto_union_o))) {
        corto_int32 discriminator = *(corto_int32*)v;
        corto_member member = safe_corto_union_findCase(t, discriminator);
        if (member) {
            if (corto_walk_member(this, member, o, v, cb, info, userData)) {
                goto error;
            }
        } else {
            /* Member not found? That means the discriminator is invalid. */
            corto_seterr("discriminator %d invalid for union '%s'\n",
                discriminator,
                corto_fullpath(NULL, t));
            goto error;
        }
    } else {
        for(i = 0; i < t->members.length; i ++) {
            m = t->members.buffer[i];
            if (corto_walk_member(this, m, o, v, cb, info, userData)) {
                goto error;
            }
        }
    }

    return 0;
error:
    return -1;
}

struct corto_serializeElement_t {
    corto_walk_opt* this;
    void* userData;
    corto_value* info;
    corto_walk_cb cb;
};

/* Serialize element */
int corto_serializeElement(void* e, void* userData) {
    struct corto_serializeElement_t* data;
    corto_walk_opt* this;
    corto_value* info;

    data = userData;
    this = data->this;
    info = data->info;

    /* Set element value */
    info->is.element.v = e;

    /* Forward element to serializer callback */
    if (data->cb(this, info, data->userData)) {
        goto error;
    }

    /* Increase index */
    info->is.element.t.index++;

    return 1;
error:
    return 0;
}

typedef struct __dummySeq {
    corto_uint32 length;
    void* buffer;
}__dummySeq;

static int corto_arrayWalk(corto_collection this, corto_void* array, corto_uint32 length, corto_elementWalk_cb action, corto_void* userData) {
    void* v;
    int result;
    corto_type elementType;
    corto_uint32 elementSize, i;

    result = 1;

    if (array) {
        elementType = this->elementType;
        elementSize = corto_type_sizeof(elementType);
        v = array;

        result = 1;
        for(i=0; (i<length) && result; i++) {
            result = action(v, userData);
            v = CORTO_OFFSET(v, elementSize);
        }
    }

    return result;
}

/* Serialize elements */
int16_t corto_walk_elements(corto_walk_opt* this, corto_value* info, void* userData) {
    struct corto_serializeElement_t walkData;
    corto_collection t;
    corto_void* v;
    corto_value elementInfo;

    t = corto_collection(corto_value_typeof(info));
    v = corto_value_ptrof(info);

    /* Value object for element */
    elementInfo.kind = CORTO_ELEMENT;
    elementInfo.is.element.o = corto_value_objectof(info);
    elementInfo.parent = info;
    elementInfo.is.element.t.type = t->elementType;
    elementInfo.is.element.t.index = 0;

    walkData.this = this;
    walkData.userData = userData;
    walkData.info = &elementInfo;

    /* Determine callback now, instead of having to do this in the element callback */
    walkData.cb = this->metaprogram[CORTO_ELEMENT];
    if (!walkData.cb) {
        walkData.cb = corto_walk_value;
    }

    int16_t result = 1;

    switch(t->kind) {
    case CORTO_ARRAY:
        result = corto_arrayWalk(t, v, t->max, corto_serializeElement, &walkData);
        break;
    case CORTO_SEQUENCE:
        result = corto_arrayWalk(t, ((__dummySeq*)v)->buffer, ((__dummySeq*)v)->length, corto_serializeElement, &walkData);
        break;
    case CORTO_LIST: {
        corto_ll list = *(corto_ll*)v;
        if (list) {
            if (corto_collection_requiresAlloc(t->elementType)) {
                result = corto_ll_walk(list, corto_serializeElement, &walkData);
            } else {
                result = corto_ll_walkPtr(list, corto_serializeElement, &walkData);
            }
        }
        break;
    }
    case CORTO_MAP: {
        corto_rbtree tree = *(corto_rbtree*)v;
        if (tree) {
            if (corto_collection_requiresAlloc(t->elementType)) {
                result = corto_rb_walk(tree, corto_serializeElement, &walkData);
            } else {
                result = corto_rb_walkPtr(tree, corto_serializeElement, &walkData);
            }
        }
        break;
    }
    }

    return !result;
}
