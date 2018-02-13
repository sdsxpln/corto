/* Copyright (c) 2010-2018 the corto developers
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

/* Public headers */
#include <corto/corto.h>

/* Private headers */
#include "bootstrap.h"
#include "object.h"
#include "cdeclhandler.h"
#include "init_ser.h"
#include "memory_ser.h"
#include "src/lang/class.h"
#include "src/lang/interface.h"

void corto_secure_init(void);

/* Declaration of the C-binding call-handler */
void corto_invoke_cdecl(corto_function f, corto_void* result, void* args);

/* TLS callback to cleanup observer administration */
void corto_observerAdminFree(void *admin);
void corto_declaredByMeFree(void *admin);

struct corto_exitHandler {
    void(*handler)(void*);
    void* userData;
};

#define VERSION_MAJOR "2"
#define VERSION_MINOR "0"
#define VERSION_PATCH "0"
#define VERSION_SUFFIX "alpha"

#ifdef VERSION_SUFFIX
const char* BAKE_VERSION = VERSION_MAJOR "." VERSION_MINOR "." VERSION_PATCH "-" VERSION_SUFFIX;
const char* BAKE_VERSION_SUFFIX = VERSION_SUFFIX;
#else
const char* BAKE_VERSION = VERSION_MAJOR "." VERSION_MINOR "." VERSION_PATCH;
const char* BAKE_VERSION_SUFFIX = "";
#endif

const char* BAKE_VERSION_MAJOR = VERSION_MAJOR;
const char* BAKE_VERSION_MINOR = VERSION_MINOR;
const char* BAKE_VERSION_PATCH = VERSION_PATCH;

/* Single lock to protect infrequent actions on global corto data */
corto_mutex_s corto_adminLock;
corto_rwmutex_s corto_subscriberLock;

/* Application name */
char *corto_appName = NULL;

/* TLS keys */
corto_tls CORTO_KEY_OBSERVER_ADMIN;
corto_tls CORTO_KEY_DECLARED_ADMIN;
corto_tls CORTO_KEY_LISTEN_ADMIN;
corto_tls CORTO_KEY_OWNER;
corto_tls CORTO_KEY_ATTR;
corto_tls CORTO_KEY_FLUENT;
corto_tls CORTO_KEY_MOUNT_RESULT;
corto_tls CORTO_KEY_CONSTRUCTOR_TYPE;

/* Delegate object variables */
corto_member corto_type_init_o = NULL;
corto_member corto_type_deinit_o = NULL;
corto_member corto_class_construct_o = NULL;
corto_member corto_class_define_o = NULL;
corto_member corto_class_destruct_o = NULL;
corto_member corto_class_delete_o = NULL;
corto_member corto_class_validate_o = NULL;
corto_member corto_class_update_o = NULL;

/* variables that control verbosity of logging functions */
int8_t CORTO_DEBUG_ENABLED = 0;

/* When set, memory management traces are enabled for this object */
corto_object CORTO_TRACE_OBJECT = NULL;
corto_string CORTO_TRACE_ID = NULL;

/* When set, notifications are traced */
int8_t CORTO_TRACE_NOTIFICATIONS = 0;

/* When set, the runtime will break at specified breakpoint */
int32_t CORTO_MEMTRACE_BREAKPOINT;

/* Package loader */
static corto_loader corto_loaderInstance;

/* Actions to be run at shutdown */
static corto_ll corto_exitHandlers = NULL;

/* String identifying current corto build */
static corto_string CORTO_BUILD = __DATE__ " " __TIME__;

#define SSO_OBJECT(obj) CORTO_OFFSET(&obj##__o, sizeof(corto_SSO))
#define SSO_OP_VOID(parent, obj) {SSO_OBJECT(parent##obj), 0}
#define SSO_OP_VALUE(parent, obj) {SSO_OBJECT(parent##obj), sizeof(corto_##obj)}
#define SSO_OP_CLASS(parent, obj) {SSO_OBJECT(parent##obj), sizeof(struct corto_##obj##_s)}
#define SSO_OP_OBJ(obj) {SSO_OBJECT(obj), 0}

/* The ordering of the lists of objects below is important to ensure correct
 * initialization\construction\destruction of objects.
 *
 * During intiialization, objects are added to the hierarchy, and their
 * intializers/constructors are called. This will (for types) calculate the type
 * sizes, which will then be verified to be the same as the actual corto type.
 *
 * The definitions of these objects are located in bootstrap.h. For an object
 * defined in bootstrap.h to be visible in the store, it needs to be added to
 * the list of to be initialized objects.
 */

/* Tier 1 objects */
#define SSO_OP_TYPE()\
    SSO_OP_VALUE(lang_, octet),\
    SSO_OP_VALUE(lang_, bool),\
    SSO_OP_VALUE(lang_, char),\
    SSO_OP_VALUE(lang_, uint8),\
    SSO_OP_VALUE(lang_, uint16),\
    SSO_OP_VALUE(lang_, uint32),\
    SSO_OP_VALUE(lang_, uint64),\
    SSO_OP_VALUE(lang_, int8),\
    SSO_OP_VALUE(lang_, int16),\
    SSO_OP_VALUE(lang_, int32),\
    SSO_OP_VALUE(lang_, int64),\
    SSO_OP_VALUE(lang_, float32),\
    SSO_OP_VALUE(lang_, float64),\
    SSO_OP_VALUE(lang_, string),\
    SSO_OP_VALUE(lang_, word),\
    SSO_OP_VALUE(lang_, constant),\
    SSO_OP_VALUE(lang_, any),\
    SSO_OP_VOID(lang_, void),\
    SSO_OP_VOID(lang_, unknown),\
    SSO_OP_VALUE(lang_, object),\
    SSO_OP_VALUE(lang_, width),\
    SSO_OP_VALUE(lang_, typeKind),\
    SSO_OP_VALUE(lang_, primitiveKind),\
    SSO_OP_VALUE(lang_, compositeKind),\
    SSO_OP_VALUE(lang_, collectionKind),\
    SSO_OP_VALUE(lang_, equalityKind),\
    SSO_OP_VALUE(lang_, inout),\
    SSO_OP_VALUE(vstore_, operatorKind),\
    SSO_OP_VALUE(vstore_, ownership),\
    SSO_OP_VALUE(vstore_, mountMask),\
    SSO_OP_VALUE(vstore_, frameKind),\
    SSO_OP_VALUE(,secure_accessKind),\
    SSO_OP_VALUE(,secure_actionKind),\
    SSO_OP_VALUE(lang_, modifier),\
    SSO_OP_VALUE(vstore_, eventMask),\
    SSO_OP_VALUE(vstore_, resultMask),\
    SSO_OP_VALUE(lang_, state),\
    SSO_OP_VALUE(lang_, attr),\
    SSO_OP_VALUE(lang_, int32seq),\
    SSO_OP_VALUE(lang_, wordseq),\
    SSO_OP_VALUE(lang_, objectseq),\
    SSO_OP_VALUE(lang_, interfaceseq),\
    SSO_OP_VALUE(lang_, parameterseq),\
    SSO_OP_VALUE(lang_, stringseq),\
    SSO_OP_VALUE(lang_, interfaceVectorseq),\
    SSO_OP_VALUE(lang_, interfaceVector),\
    SSO_OP_VALUE(lang_, objectlist),\
    SSO_OP_VALUE(lang_, taglist),\
    SSO_OP_VALUE(lang_, stringlist),\
    SSO_OP_VALUE(vstore_, resultList),\
    SSO_OP_VALUE(vstore_, mountSubscriptionList),\
    SSO_OP_VALUE(lang_, parameter),\
    SSO_OP_VALUE(lang_, typeOptions),\
    SSO_OP_VALUE(vstore_, time),\
    SSO_OP_VALUE(vstore_, frame),\
    SSO_OP_VALUE(vstore_, sample),\
    SSO_OP_VALUE(vstore_, sampleIter),\
    SSO_OP_VALUE(vstore_, subscriber_eventIter),\
    SSO_OP_VALUE(vstore_, result),\
    SSO_OP_VALUE(vstore_, queuePolicy),\
    SSO_OP_VALUE(vstore_, mountPolicy),\
    SSO_OP_VALUE(lang_, delegatedata),\
    SSO_OP_VOID(vstore_, dispatcher),\
    SSO_OP_VALUE(lang_, pre_action),\
    SSO_OP_VALUE(lang_, name_action),\
    SSO_OP_VALUE(lang_, post_action),\
    SSO_OP_VALUE(vstore_, handleAction),\
    SSO_OP_VALUE(vstore_, resultIter),\
    SSO_OP_VALUE(vstore_, objectIter),\
    SSO_OP_VALUE(vstore_, query),\
    SSO_OP_VALUE(vstore_, mountSubscription),\
    SSO_OP_CLASS(lang_, function),\
    SSO_OP_CLASS(lang_, method),\
    SSO_OP_CLASS(lang_, overridable),\
    SSO_OP_CLASS(lang_, override),\
    SSO_OP_CLASS(vstore_, remote),\
    SSO_OP_CLASS(vstore_, observer),\
    SSO_OP_CLASS(vstore_, subscriber),\
    SSO_OP_CLASS(lang_, metaprocedure),\
    SSO_OP_CLASS(vstore_, route),\
    SSO_OP_CLASS(lang_, type),\
    SSO_OP_CLASS(lang_, primitive),\
    SSO_OP_CLASS(lang_, interface),\
    SSO_OP_CLASS(lang_, collection),\
    SSO_OP_CLASS(lang_, iterator),\
    SSO_OP_CLASS(lang_, struct),\
    SSO_OP_CLASS(lang_, union),\
    SSO_OP_VALUE(vstore_, event),\
    SSO_OP_VALUE(vstore_, fmt_data),\
    SSO_OP_VALUE(vstore_, observer_event),\
    SSO_OP_VALUE(vstore_, subscriber_event),\
    SSO_OP_VALUE(vstore_, invokeEvent),\
    SSO_OP_CLASS(lang_, binary),\
    SSO_OP_CLASS(lang_, boolean),\
    SSO_OP_CLASS(lang_, character),\
    SSO_OP_CLASS(lang_, int),\
    SSO_OP_CLASS(lang_, uint),\
    SSO_OP_CLASS(lang_, float),\
    SSO_OP_CLASS(lang_, text),\
    SSO_OP_CLASS(lang_, verbatim),\
    SSO_OP_CLASS(lang_, enum),\
    SSO_OP_CLASS(lang_, bitmask),\
    SSO_OP_CLASS(lang_, array),\
    SSO_OP_CLASS(lang_, sequence),\
    SSO_OP_CLASS(lang_, list),\
    SSO_OP_CLASS(lang_, map),\
    SSO_OP_CLASS(lang_, member),\
    SSO_OP_CLASS(lang_, case),\
    SSO_OP_CLASS(lang_, default),\
    SSO_OP_CLASS(lang_, alias),\
    SSO_OP_CLASS(lang_, class),\
    SSO_OP_CLASS(lang_, container),\
    SSO_OP_CLASS(lang_, leaf),\
    SSO_OP_CLASS(lang_, table),\
    SSO_OP_CLASS(lang_, tableinstance),\
    SSO_OP_CLASS(lang_, procedure),\
    SSO_OP_CLASS(lang_, delegate),\
    SSO_OP_CLASS(lang_, target),\
    SSO_OP_CLASS(lang_, quantity),\
    SSO_OP_CLASS(lang_, unit),\
    SSO_OP_CLASS(lang_, tag),\
    SSO_OP_CLASS(lang_, package),\
    SSO_OP_CLASS(lang_, application),\
    SSO_OP_CLASS(lang_, tool),\
    SSO_OP_CLASS(vstore_, router),\
    SSO_OP_CLASS(vstore_, routerimpl),\
    SSO_OP_CLASS(vstore_, mount),\
    SSO_OP_CLASS(vstore_, loader),\
    SSO_OP_CLASS(,native_type),\
    SSO_OP_CLASS(,secure_key),\
    SSO_OP_CLASS(,secure_lock)

/* Tier 2 objects */
#define SSO_OP_OBJECT()\
    SSO_OP_OBJ(lang_class_construct_),\
    SSO_OP_OBJ(lang_class_destruct_),\
    /* constant */\
    SSO_OP_OBJ(lang_constant_init_),\
    /* function */\
    SSO_OP_OBJ(lang_function_returnType),\
    SSO_OP_OBJ(lang_function_returnsReference),\
    SSO_OP_OBJ(lang_function_parameters),\
    SSO_OP_OBJ(lang_function_overridable),\
    SSO_OP_OBJ(lang_function_overloaded),\
    SSO_OP_OBJ(lang_function_kind),\
    SSO_OP_OBJ(lang_function_impl),\
    SSO_OP_OBJ(lang_function_fptr),\
    SSO_OP_OBJ(lang_function_fdata),\
    SSO_OP_OBJ(lang_function_size),\
    SSO_OP_OBJ(lang_function_init_),\
    SSO_OP_OBJ(lang_function_construct_),\
    SSO_OP_OBJ(lang_function_destruct_),\
    SSO_OP_OBJ(lang_function_stringToParameterSeq),\
    SSO_OP_OBJ(lang_function_parseParamString_),\
    /* method */\
    SSO_OP_OBJ(lang_method_index),\
    /* overridable */\
    SSO_OP_OBJ(lang_overridable_init_),\
    /* observer */\
    SSO_OP_OBJ(vstore_observer_mask),\
    SSO_OP_OBJ(vstore_observer_observable),\
    SSO_OP_OBJ(vstore_observer_instance),\
    SSO_OP_OBJ(vstore_observer_dispatcher),\
    SSO_OP_OBJ(vstore_observer_type),\
    SSO_OP_OBJ(vstore_observer_enabled),\
    SSO_OP_OBJ(vstore_observer_active),\
    SSO_OP_OBJ(vstore_observer_init_),\
    SSO_OP_OBJ(vstore_observer_construct_),\
    SSO_OP_OBJ(vstore_observer_destruct_),\
    SSO_OP_OBJ(vstore_observer_observe_),\
    SSO_OP_OBJ(vstore_observer_unobserve_),\
    SSO_OP_OBJ(vstore_observer_observing_),\
    /* metaprocedure */\
    SSO_OP_OBJ(lang_metaprocedure_referenceOnly),\
    SSO_OP_OBJ(lang_metaprocedure_construct_),\
    /* route */\
    SSO_OP_OBJ(vstore_route_pattern),\
    SSO_OP_OBJ(vstore_route_elements),\
    SSO_OP_OBJ(vstore_route_init_),\
    SSO_OP_OBJ(vstore_route_construct_),\
    /* dispatcher */\
    SSO_OP_OBJ(vstore_dispatcher_post),\
    /* event */\
    SSO_OP_OBJ(vstore_event_handleAction),\
    SSO_OP_OBJ(vstore_event_handle_),\
    /* fmt_data */\
    SSO_OP_OBJ(vstore_fmt_data_ptr),\
    SSO_OP_OBJ(vstore_fmt_data_handle),\
    SSO_OP_OBJ(vstore_fmt_data_shared_count),\
    SSO_OP_OBJ(vstore_fmt_data_deinit_),\
    /* observer_event */\
    SSO_OP_OBJ(vstore_observer_event_observer),\
    SSO_OP_OBJ(vstore_observer_event_instance),\
    SSO_OP_OBJ(vstore_observer_event_source),\
    SSO_OP_OBJ(vstore_observer_event_event),\
    SSO_OP_OBJ(vstore_observer_event_data),\
    SSO_OP_OBJ(vstore_observer_event_thread),\
    SSO_OP_OBJ(vstore_observer_event_handle),\
    SSO_OP_OBJ(vstore_observer_event_init_),\
    SSO_OP_OBJ(vstore_observer_event_deinit_),\
    /* subscriber_event */\
    SSO_OP_OBJ(vstore_subscriber_event_subscriber),\
    SSO_OP_OBJ(vstore_subscriber_event_instance),\
    SSO_OP_OBJ(vstore_subscriber_event_source),\
    SSO_OP_OBJ(vstore_subscriber_event_event),\
    SSO_OP_OBJ(vstore_subscriber_event_data),\
    SSO_OP_OBJ(vstore_subscriber_event_fmt),\
    SSO_OP_OBJ(vstore_subscriber_event_handle),\
    SSO_OP_OBJ(vstore_subscriber_event_init_),\
    SSO_OP_OBJ(vstore_subscriber_event_deinit_),\
    /* invokeEvent */\
    SSO_OP_OBJ(vstore_invokeEvent_mount),\
    SSO_OP_OBJ(vstore_invokeEvent_instance),\
    SSO_OP_OBJ(vstore_invokeEvent_function),\
    SSO_OP_OBJ(vstore_invokeEvent_args),\
    SSO_OP_OBJ(vstore_invokeEvent_handle_),\
    /* width */\
    SSO_OP_OBJ(lang_width_WIDTH_8),\
    SSO_OP_OBJ(lang_width_WIDTH_16),\
    SSO_OP_OBJ(lang_width_WIDTH_32),\
    SSO_OP_OBJ(lang_width_WIDTH_64),\
    SSO_OP_OBJ(lang_width_WIDTH_WORD),\
    /* typeKind */\
    SSO_OP_OBJ(lang_typeKind_VOID),\
    SSO_OP_OBJ(lang_typeKind_ANY),\
    SSO_OP_OBJ(lang_typeKind_PRIMITIVE),\
    SSO_OP_OBJ(lang_typeKind_COMPOSITE),\
    SSO_OP_OBJ(lang_typeKind_COLLECTION),\
    SSO_OP_OBJ(lang_typeKind_ITERATOR),\
    /* primitiveKind */\
    SSO_OP_OBJ(lang_primitiveKind_BINARY),\
    SSO_OP_OBJ(lang_primitiveKind_BOOLEAN),\
    SSO_OP_OBJ(lang_primitiveKind_CHARACTER),\
    SSO_OP_OBJ(lang_primitiveKind_INTEGER),\
    SSO_OP_OBJ(lang_primitiveKind_UINTEGER),\
    SSO_OP_OBJ(lang_primitiveKind_FLOAT),\
    SSO_OP_OBJ(lang_primitiveKind_TEXT),\
    SSO_OP_OBJ(lang_primitiveKind_ENUM),\
    SSO_OP_OBJ(lang_primitiveKind_BITMASK),\
    /* compositeKind */\
    SSO_OP_OBJ(lang_compositeKind_INTERFACE),\
    SSO_OP_OBJ(lang_compositeKind_STRUCT),\
    SSO_OP_OBJ(lang_compositeKind_UNION),\
    SSO_OP_OBJ(lang_compositeKind_CLASS),\
    SSO_OP_OBJ(lang_compositeKind_DELEGATE),\
    SSO_OP_OBJ(lang_compositeKind_PROCEDURE),\
    /* collectionKind */\
    SSO_OP_OBJ(lang_collectionKind_ARRAY),\
    SSO_OP_OBJ(lang_collectionKind_SEQUENCE),\
    SSO_OP_OBJ(lang_collectionKind_LIST),\
    SSO_OP_OBJ(lang_collectionKind_MAP),\
    /* equalityKind */\
    SSO_OP_OBJ(lang_equalityKind_EQ),\
    SSO_OP_OBJ(lang_equalityKind_LT),\
    SSO_OP_OBJ(lang_equalityKind_GT),\
    SSO_OP_OBJ(lang_equalityKind_NEQ),\
    /* inout */\
    SSO_OP_OBJ(lang_inout_IN),\
    SSO_OP_OBJ(lang_inout_OUT),\
    SSO_OP_OBJ(lang_inout_INOUT),\
    /* ownership */\
    SSO_OP_OBJ(vstore_ownership_REMOTE_SOURCE),\
    SSO_OP_OBJ(vstore_ownership_LOCAL_SOURCE),\
    SSO_OP_OBJ(vstore_ownership_CACHE_OWNER),\
    /* readWrite */\
    SSO_OP_OBJ(vstore_mountMask_MOUNT_QUERY),\
    SSO_OP_OBJ(vstore_mountMask_MOUNT_HISTORY_QUERY),\
    SSO_OP_OBJ(vstore_mountMask_MOUNT_NOTIFY),\
    SSO_OP_OBJ(vstore_mountMask_MOUNT_HISTORY_BATCH_NOTIFY),\
    SSO_OP_OBJ(vstore_mountMask_MOUNT_BATCH_NOTIFY),\
    SSO_OP_OBJ(vstore_mountMask_MOUNT_SUBSCRIBE),\
    SSO_OP_OBJ(vstore_mountMask_MOUNT_MOUNT),\
    SSO_OP_OBJ(vstore_mountMask_MOUNT_RESUME),\
    SSO_OP_OBJ(vstore_mountMask_MOUNT_INVOKE),\
    SSO_OP_OBJ(vstore_mountMask_MOUNT_ID),\
    /* frameKind */\
    SSO_OP_OBJ(vstore_frameKind_FRAME_NOW),\
    SSO_OP_OBJ(vstore_frameKind_FRAME_TIME),\
    SSO_OP_OBJ(vstore_frameKind_FRAME_DURATION),\
    /* accessKind */\
    SSO_OP_OBJ(secure_accessKind_SECURE_ACCESS_GRANTED),\
    SSO_OP_OBJ(secure_accessKind_SECURE_ACCESS_DENIED),\
    SSO_OP_OBJ(secure_accessKind_SECURE_ACCESS_UNDEFINED),\
    /* actionKind */\
    SSO_OP_OBJ(secure_actionKind_SECURE_ACTION_CREATE),\
    SSO_OP_OBJ(secure_actionKind_SECURE_ACTION_READ),\
    SSO_OP_OBJ(secure_actionKind_SECURE_ACTION_UPDATE),\
    SSO_OP_OBJ(secure_actionKind_SECURE_ACTION_DELETE),\
    /* operatorKind */\
    SSO_OP_OBJ(vstore_operatorKind_ASSIGN),\
    SSO_OP_OBJ(vstore_operatorKind_ASSIGN_ADD),\
    SSO_OP_OBJ(vstore_operatorKind_ASSIGN_SUB),\
    SSO_OP_OBJ(vstore_operatorKind_ASSIGN_MUL),\
    SSO_OP_OBJ(vstore_operatorKind_ASSIGN_DIV),\
    SSO_OP_OBJ(vstore_operatorKind_ASSIGN_MOD),\
    SSO_OP_OBJ(vstore_operatorKind_ASSIGN_XOR),\
    SSO_OP_OBJ(vstore_operatorKind_ASSIGN_OR),\
    SSO_OP_OBJ(vstore_operatorKind_ASSIGN_AND),\
    SSO_OP_OBJ(vstore_operatorKind_ASSIGN_UPDATE),\
    SSO_OP_OBJ(vstore_operatorKind_ADD),\
    SSO_OP_OBJ(vstore_operatorKind_SUB),\
    SSO_OP_OBJ(vstore_operatorKind_MUL),\
    SSO_OP_OBJ(vstore_operatorKind_DIV),\
    SSO_OP_OBJ(vstore_operatorKind_MOD),\
    SSO_OP_OBJ(vstore_operatorKind_INC),\
    SSO_OP_OBJ(vstore_operatorKind_DEC),\
    SSO_OP_OBJ(vstore_operatorKind_XOR),\
    SSO_OP_OBJ(vstore_operatorKind_OR),\
    SSO_OP_OBJ(vstore_operatorKind_AND),\
    SSO_OP_OBJ(vstore_operatorKind_NOT),\
    SSO_OP_OBJ(vstore_operatorKind_COND_OR),\
    SSO_OP_OBJ(vstore_operatorKind_COND_AND),\
    SSO_OP_OBJ(vstore_operatorKind_COND_NOT),\
    SSO_OP_OBJ(vstore_operatorKind_COND_EQ),\
    SSO_OP_OBJ(vstore_operatorKind_COND_NEQ),\
    SSO_OP_OBJ(vstore_operatorKind_COND_GT),\
    SSO_OP_OBJ(vstore_operatorKind_COND_LT),\
    SSO_OP_OBJ(vstore_operatorKind_COND_GTEQ),\
    SSO_OP_OBJ(vstore_operatorKind_COND_LTEQ),\
    SSO_OP_OBJ(vstore_operatorKind_SHIFT_LEFT),\
    SSO_OP_OBJ(vstore_operatorKind_SHIFT_RIGHT),\
    SSO_OP_OBJ(vstore_operatorKind_REF),\
    /* state */\
    SSO_OP_OBJ(lang_state_VALID),\
    SSO_OP_OBJ(lang_state_DELETED),\
    SSO_OP_OBJ(lang_state_DECLARED),\
    /* attr */\
    SSO_OP_OBJ(lang_attr_ATTR_NAMED),\
    SSO_OP_OBJ(lang_attr_ATTR_WRITABLE),\
    SSO_OP_OBJ(lang_attr_ATTR_OBSERVABLE),\
    SSO_OP_OBJ(lang_attr_ATTR_PERSISTENT),\
    SSO_OP_OBJ(lang_attr_ATTR_DEFAULT),\
    /* eventKind */\
    SSO_OP_OBJ(vstore_eventMask_DECLARE),\
    SSO_OP_OBJ(vstore_eventMask_DEFINE),\
    SSO_OP_OBJ(vstore_eventMask_DELETE),\
    SSO_OP_OBJ(vstore_eventMask_INVALIDATE),\
    SSO_OP_OBJ(vstore_eventMask_UPDATE),\
    SSO_OP_OBJ(vstore_eventMask_RESUME),\
    SSO_OP_OBJ(vstore_eventMask_SUSPEND),\
    SSO_OP_OBJ(vstore_eventMask_ON_SELF),\
    SSO_OP_OBJ(vstore_eventMask_ON_SCOPE),\
    SSO_OP_OBJ(vstore_eventMask_ON_TREE),\
    SSO_OP_OBJ(vstore_eventMask_ON_VALUE),\
    SSO_OP_OBJ(vstore_eventMask_ON_METAVALUE),\
    SSO_OP_OBJ(vstore_eventMask_ON_ANY),\
    /* modifier */\
    SSO_OP_OBJ(lang_modifier_GLOBAL),\
    SSO_OP_OBJ(lang_modifier_LOCAL),\
    SSO_OP_OBJ(lang_modifier_PRIVATE),\
    SSO_OP_OBJ(lang_modifier_READONLY),\
    SSO_OP_OBJ(lang_modifier_CONST),\
    SSO_OP_OBJ(lang_modifier_NOT_NULL),\
    SSO_OP_OBJ(lang_modifier_HIDDEN),\
    SSO_OP_OBJ(lang_modifier_OPTIONAL),\
    SSO_OP_OBJ(lang_modifier_OBSERVABLE),\
    SSO_OP_OBJ(lang_modifier_KEY),\
    /* resultMask */\
    SSO_OP_OBJ(vstore_resultMask_RESULT_LEAF),\
    SSO_OP_OBJ(vstore_resultMask_RESULT_HIDDEN),\
    /* typeOptions */\
    SSO_OP_OBJ(lang_typeOptions_parentType),\
    SSO_OP_OBJ(lang_typeOptions_parentState),\
    SSO_OP_OBJ(lang_typeOptions_defaultType),\
    SSO_OP_OBJ(lang_typeOptions_defaultProcedureType),\
    /* type */\
    SSO_OP_OBJ(lang_type_kind),\
    SSO_OP_OBJ(lang_type_reference),\
    SSO_OP_OBJ(lang_type_attr),\
    SSO_OP_OBJ(lang_type_options),\
    SSO_OP_OBJ(lang_type_flags),\
    SSO_OP_OBJ(lang_type_size),\
    SSO_OP_OBJ(lang_type_alignment),\
    SSO_OP_OBJ(lang_type_metaprocedures),\
    SSO_OP_OBJ(lang_type_init),\
    SSO_OP_OBJ(lang_type_deinit),\
    SSO_OP_OBJ(lang_type_nameof),\
    SSO_OP_OBJ(lang_type_sizeof_),\
    SSO_OP_OBJ(lang_type_alignmentof_),\
    SSO_OP_OBJ(lang_type_compatible_),\
    SSO_OP_OBJ(lang_type_resolveProcedure_),\
    SSO_OP_OBJ(lang_type_castable_),\
    SSO_OP_OBJ(lang_type_init_),\
    SSO_OP_OBJ(lang_type_construct_),\
    SSO_OP_OBJ(lang_type_destruct_),\
    /* primitive */\
    SSO_OP_OBJ(lang_primitive_kind),\
    SSO_OP_OBJ(lang_primitive_width),\
    SSO_OP_OBJ(lang_primitive_convertId),\
    SSO_OP_OBJ(lang_primitive_init_),\
    SSO_OP_OBJ(lang_primitive_construct_),\
    SSO_OP_OBJ(lang_primitive_compatible_),\
    SSO_OP_OBJ(lang_primitive_castable_),\
    SSO_OP_OBJ(lang_primitive_isInteger_),\
    SSO_OP_OBJ(lang_primitive_isNumber_),\
    /* interface */\
    SSO_OP_OBJ(lang_interface_kind),\
    SSO_OP_OBJ(lang_interface_nextMemberId),\
    SSO_OP_OBJ(lang_interface_members),\
    SSO_OP_OBJ(lang_interface_methods),\
    SSO_OP_OBJ(lang_interface_base),\
    SSO_OP_OBJ(lang_interface_init_),\
    SSO_OP_OBJ(lang_interface_construct_),\
    SSO_OP_OBJ(lang_interface_destruct_),\
    SSO_OP_OBJ(lang_interface_resolveMember_),\
    SSO_OP_OBJ(lang_interface_resolveMemberByTag_),\
    SSO_OP_OBJ(lang_interface_compatible_),\
    SSO_OP_OBJ(lang_interface_resolveMethod_),\
    SSO_OP_OBJ(lang_interface_resolveMethodId_),\
    SSO_OP_OBJ(lang_interface_resolveMethodById_),\
    SSO_OP_OBJ(lang_interface_bindMethod_),\
    SSO_OP_OBJ(lang_interface_baseof_),\
    /* collection */\
    SSO_OP_OBJ(lang_collection_kind),\
    SSO_OP_OBJ(lang_collection_elementType),\
    SSO_OP_OBJ(lang_collection_max),\
    SSO_OP_OBJ(lang_collection_castable_),\
    SSO_OP_OBJ(lang_collection_compatible_),\
    SSO_OP_OBJ(lang_collection_requiresAlloc),\
    SSO_OP_OBJ(lang_collection_init_),\
    /* iterator */\
    SSO_OP_OBJ(lang_iterator_elementType),\
    SSO_OP_OBJ(lang_iterator_init_),\
    SSO_OP_OBJ(lang_iterator_castable_),\
    SSO_OP_OBJ(lang_iterator_compatible_),\
    /* binary */\
    SSO_OP_OBJ(lang_binary_init_),\
    /* boolean */\
    SSO_OP_OBJ(lang_boolean_init_),\
    /* char */\
    SSO_OP_OBJ(lang_character_init_),\
    /* int */\
    SSO_OP_OBJ(lang_int_min),\
    SSO_OP_OBJ(lang_int_max),\
    SSO_OP_OBJ(lang_int_init_),\
    /* uint */\
    SSO_OP_OBJ(lang_uint_min),\
    SSO_OP_OBJ(lang_uint_max),\
    SSO_OP_OBJ(lang_uint_init_),\
    /* float */\
    SSO_OP_OBJ(lang_float_min),\
    SSO_OP_OBJ(lang_float_max),\
    SSO_OP_OBJ(lang_float_init_),\
    /* text */\
    SSO_OP_OBJ(lang_text_charWidth),\
    SSO_OP_OBJ(lang_text_length),\
    SSO_OP_OBJ(lang_text_init_),\
    /* verbatim */\
    SSO_OP_OBJ(lang_verbatim_contentType),\
    SSO_OP_OBJ(lang_verbatim_init_),\
    /* enum */\
    SSO_OP_OBJ(lang_enum_constants),\
    SSO_OP_OBJ(lang_enum_constant_),\
    SSO_OP_OBJ(lang_enum_init_),\
    SSO_OP_OBJ(lang_enum_construct_),\
    SSO_OP_OBJ(lang_enum_destruct_),\
    /* bitmask */\
    SSO_OP_OBJ(lang_bitmask_init_),\
    /* struct */\
    SSO_OP_OBJ(lang_struct_base),\
    SSO_OP_OBJ(lang_struct_baseAccess),\
    SSO_OP_OBJ(lang_struct_keys),\
    SSO_OP_OBJ(lang_struct_keycache),\
    SSO_OP_OBJ(lang_struct_freeops),\
    SSO_OP_OBJ(lang_struct_init_),\
    SSO_OP_OBJ(lang_struct_construct_),\
    SSO_OP_OBJ(lang_struct_destruct_),\
    SSO_OP_OBJ(lang_struct_compatible_),\
    SSO_OP_OBJ(lang_struct_castable_),\
    SSO_OP_OBJ(lang_struct_resolveMember_),\
    /* union */\
    SSO_OP_OBJ(lang_union_discriminator),\
    SSO_OP_OBJ(lang_union_init_),\
    SSO_OP_OBJ(lang_union_construct_),\
    SSO_OP_OBJ(lang_union_findCase_),\
    /* procedure */\
    SSO_OP_OBJ(lang_procedure_hasThis),\
    SSO_OP_OBJ(lang_procedure_thisType),\
    SSO_OP_OBJ(lang_procedure_init_),\
    SSO_OP_OBJ(lang_procedure_construct_),\
    /* interfaceVector */\
    SSO_OP_OBJ(lang_interfaceVector_interface),\
    SSO_OP_OBJ(lang_interfaceVector_vector),\
    /* class */\
    SSO_OP_OBJ(lang_class_base),\
    SSO_OP_OBJ(lang_class_baseAccess),\
    SSO_OP_OBJ(lang_class_implements),\
    SSO_OP_OBJ(lang_class_interfaceVector),\
    SSO_OP_OBJ(lang_class_construct),\
    SSO_OP_OBJ(lang_class_define),\
    SSO_OP_OBJ(lang_class_validate),\
    SSO_OP_OBJ(lang_class_update),\
    SSO_OP_OBJ(lang_class_destruct),\
    SSO_OP_OBJ(lang_class_delete),\
    SSO_OP_OBJ(lang_class_init_),\
    SSO_OP_OBJ(lang_class_instanceof_),\
    SSO_OP_OBJ(lang_class_resolveInterfaceMethod_),\
    /* leaf */\
    SSO_OP_OBJ(lang_container_construct_),\
    SSO_OP_OBJ(lang_container_type),\
    SSO_OP_OBJ(lang_container_value),\
    /* tableinstance */\
    SSO_OP_OBJ(lang_tableinstance_type),\
    /* table */\
    SSO_OP_OBJ(lang_table_construct_),\
    /* queuePolicy */\
    SSO_OP_OBJ(vstore_queuePolicy_max),\
    /* mountPolicy */\
    SSO_OP_OBJ(vstore_mountPolicy_ownership),\
    SSO_OP_OBJ(vstore_mountPolicy_mask),\
    SSO_OP_OBJ(vstore_mountPolicy_sampleRate),\
    SSO_OP_OBJ(vstore_mountPolicy_queue),\
    SSO_OP_OBJ(vstore_mountPolicy_expiryTime),\
    SSO_OP_OBJ(vstore_mountPolicy_filterResults),\
    /* mountSubscription */\
    SSO_OP_OBJ(vstore_mountSubscription_query),\
    SSO_OP_OBJ(vstore_mountSubscription_mountCount),\
    SSO_OP_OBJ(vstore_mountSubscription_subscriberCount),\
    SSO_OP_OBJ(vstore_mountSubscription_mountCtx),\
    SSO_OP_OBJ(vstore_mountSubscription_subscriberCtx),\
    /* query */\
    SSO_OP_OBJ(vstore_query_select),\
    SSO_OP_OBJ(vstore_query_from),\
    SSO_OP_OBJ(vstore_query_type),\
    SSO_OP_OBJ(vstore_query_member),\
    SSO_OP_OBJ(vstore_query_where),\
    SSO_OP_OBJ(vstore_query_offset),\
    SSO_OP_OBJ(vstore_query_limit),\
    SSO_OP_OBJ(vstore_query_soffset),\
    SSO_OP_OBJ(vstore_query_slimit),\
    SSO_OP_OBJ(vstore_query_timeBegin),\
    SSO_OP_OBJ(vstore_query_timeEnd),\
    SSO_OP_OBJ(vstore_query_content),\
    SSO_OP_OBJ(vstore_query_cardinality_),\
    SSO_OP_OBJ(vstore_query_match_),\
    /* subscriber */\
    SSO_OP_OBJ(vstore_subscriber_query),\
    SSO_OP_OBJ(vstore_subscriber_contentType),\
    SSO_OP_OBJ(vstore_subscriber_instance),\
    SSO_OP_OBJ(vstore_subscriber_dispatcher),\
    SSO_OP_OBJ(vstore_subscriber_enabled),\
    SSO_OP_OBJ(vstore_subscriber_fmt_handle),\
    SSO_OP_OBJ(vstore_subscriber_idmatch),\
    SSO_OP_OBJ(vstore_subscriber_isAligning),\
    SSO_OP_OBJ(vstore_subscriber_alignMutex),\
    SSO_OP_OBJ(vstore_subscriber_alignQueue),\
    SSO_OP_OBJ(vstore_subscriber_init_),\
    SSO_OP_OBJ(vstore_subscriber_deinit_),\
    SSO_OP_OBJ(vstore_subscriber_construct_),\
    SSO_OP_OBJ(vstore_subscriber_define_),\
    SSO_OP_OBJ(vstore_subscriber_destruct_),\
    SSO_OP_OBJ(vstore_subscriber_subscribe_),\
    SSO_OP_OBJ(vstore_subscriber_unsubscribe_),\
    /* router */\
    SSO_OP_OBJ(vstore_router_init_),\
    SSO_OP_OBJ(vstore_router_construct_),\
    SSO_OP_OBJ(vstore_router_match),\
    SSO_OP_OBJ(vstore_router_returnType),\
    SSO_OP_OBJ(vstore_router_paramType),\
    SSO_OP_OBJ(vstore_router_paramName),\
    SSO_OP_OBJ(vstore_router_routerDataType),\
    SSO_OP_OBJ(vstore_router_routerDataName),\
    SSO_OP_OBJ(vstore_router_elementSeparator),\
    /* routerimpl */\
    SSO_OP_OBJ(vstore_routerimpl_construct_),\
    SSO_OP_OBJ(vstore_routerimpl_destruct_),\
    SSO_OP_OBJ(vstore_routerimpl_maxArgs),\
    SSO_OP_OBJ(vstore_routerimpl_matched),\
    SSO_OP_OBJ(vstore_routerimpl_matchRoute_),\
    SSO_OP_OBJ(vstore_routerimpl_findRoute_),\
    /* mount */\
    SSO_OP_OBJ(vstore_mount_query),\
    SSO_OP_OBJ(vstore_mount_contentType),\
    SSO_OP_OBJ(vstore_mount_policy),\
    SSO_OP_OBJ(vstore_mount_mount),\
    SSO_OP_OBJ(vstore_mount_attr),\
    SSO_OP_OBJ(vstore_mount_subscriptions),\
    SSO_OP_OBJ(vstore_mount_events),\
    SSO_OP_OBJ(vstore_mount_historicalEvents),\
    SSO_OP_OBJ(vstore_mount_lastPoll),\
    SSO_OP_OBJ(vstore_mount_lastPost),\
    SSO_OP_OBJ(vstore_mount_lastSleep),\
    SSO_OP_OBJ(vstore_mount_dueSleep),\
    SSO_OP_OBJ(vstore_mount_lastQueueSize),\
    SSO_OP_OBJ(vstore_mount_passThrough),\
    SSO_OP_OBJ(vstore_mount_explicitResume),\
    SSO_OP_OBJ(vstore_mount_thread),\
    SSO_OP_OBJ(vstore_mount_quit),\
    SSO_OP_OBJ(vstore_mount_contentTypeOut),\
    SSO_OP_OBJ(vstore_mount_contentTypeOutHandle),\
    SSO_OP_OBJ(vstore_mount_init_),\
    SSO_OP_OBJ(vstore_mount_construct_),\
    SSO_OP_OBJ(vstore_mount_destruct_),\
    SSO_OP_OBJ(vstore_mount_invoke_),\
    SSO_OP_OBJ(vstore_mount_id_),\
    SSO_OP_OBJ(vstore_mount_query_),\
    SSO_OP_OBJ(vstore_mount_historyQuery_),\
    SSO_OP_OBJ(vstore_mount_resume_),\
    SSO_OP_OBJ(vstore_mount_subscribe_),\
    SSO_OP_OBJ(vstore_mount_unsubscribe_),\
    SSO_OP_OBJ(vstore_mount_setContentType_),\
    SSO_OP_OBJ(vstore_mount_setContentTypeIn_),\
    SSO_OP_OBJ(vstore_mount_setContentTypeOut_),\
    SSO_OP_OBJ(vstore_mount_return_),\
    SSO_OP_OBJ(vstore_mount_publish_),\
    SSO_OP_OBJ(vstore_mount_post_),\
    SSO_OP_OBJ(vstore_mount_onPoll_),\
    SSO_OP_OBJ(vstore_mount_on_notify_),\
    SSO_OP_OBJ(vstore_mount_on_batch_notify_),\
    SSO_OP_OBJ(vstore_mount_on_history_batch_notify_),\
    SSO_OP_OBJ(vstore_mount_on_invoke_),\
    SSO_OP_OBJ(vstore_mount_on_id_),\
    SSO_OP_OBJ(vstore_mount_on_query_),\
    SSO_OP_OBJ(vstore_mount_on_history_query_),\
    SSO_OP_OBJ(vstore_mount_on_resume_),\
    SSO_OP_OBJ(vstore_mount_on_subscribe_),\
    SSO_OP_OBJ(vstore_mount_on_unsubscribe_),\
    SSO_OP_OBJ(vstore_mount_on_mount_),\
    SSO_OP_OBJ(vstore_mount_on_unmount_),\
    SSO_OP_OBJ(vstore_mount_on_transaction_begin_),\
    SSO_OP_OBJ(vstore_mount_on_transaction_end_),\
    /* loader */\
    SSO_OP_OBJ(vstore_loader_autoLoad),\
    SSO_OP_OBJ(vstore_loader_construct_),\
    SSO_OP_OBJ(vstore_loader_destruct_),\
    SSO_OP_OBJ(vstore_loader_on_query_),\
    /* delegatedata */\
    SSO_OP_OBJ(lang_delegatedata_instance),\
    SSO_OP_OBJ(lang_delegatedata_procedure),\
    /* delegate */\
    SSO_OP_OBJ(lang_delegate_returnType),\
    SSO_OP_OBJ(lang_delegate_returnsReference),\
    SSO_OP_OBJ(lang_delegate_parameters),\
    SSO_OP_OBJ(lang_delegate_init_),\
    SSO_OP_OBJ(lang_delegate_compatible_),\
    SSO_OP_OBJ(lang_delegate_castable_),\
    SSO_OP_OBJ(lang_delegate_instanceof_),\
    SSO_OP_OBJ(lang_delegate_bind),\
    /* target */\
    SSO_OP_OBJ(lang_target_type),\
    SSO_OP_OBJ(lang_target_construct_),\
    /* quantity */\
    SSO_OP_OBJ(lang_quantity_base_unit),\
    /* tag */\
    /* unit */\
    SSO_OP_OBJ(lang_unit_quantity),\
    SSO_OP_OBJ(lang_unit_symbol),\
    SSO_OP_OBJ(lang_unit_conversion),\
    SSO_OP_OBJ(lang_unit_type),\
    SSO_OP_OBJ(lang_unit_toQuantity),\
    SSO_OP_OBJ(lang_unit_fromQuantity),\
    SSO_OP_OBJ(lang_unit_init_),\
    SSO_OP_OBJ(lang_unit_construct_),\
    /* array */\
    SSO_OP_OBJ(lang_array_elementType),\
    SSO_OP_OBJ(lang_array_init_),\
    SSO_OP_OBJ(lang_array_construct_),\
    SSO_OP_OBJ(lang_array_destruct_),\
    /* sequence */\
    SSO_OP_OBJ(lang_sequence_init_),\
    SSO_OP_OBJ(lang_sequence_construct_),\
    /* list */\
    SSO_OP_OBJ(lang_list_init_),\
    SSO_OP_OBJ(lang_list_construct_),\
    /* map */\
    SSO_OP_OBJ(lang_map_keyType),\
    SSO_OP_OBJ(lang_map_elementType),\
    SSO_OP_OBJ(lang_map_max),\
    SSO_OP_OBJ(lang_map_init_),\
    SSO_OP_OBJ(lang_map_construct_),\
    /* member */\
    SSO_OP_OBJ(lang_member_type),\
    SSO_OP_OBJ(lang_member_modifiers),\
    SSO_OP_OBJ(lang_member_default),\
    SSO_OP_OBJ(lang_member_unit),\
    SSO_OP_OBJ(lang_member_tags),\
    SSO_OP_OBJ(lang_member_state),\
    SSO_OP_OBJ(lang_member_stateCondExpr),\
    SSO_OP_OBJ(lang_member_id),\
    SSO_OP_OBJ(lang_member_offset),\
    SSO_OP_OBJ(lang_member_init_),\
    SSO_OP_OBJ(lang_member_construct_),\
    /* alias */\
    SSO_OP_OBJ(lang_alias_member),\
    SSO_OP_OBJ(lang_alias_construct_),\
    /* case */\
    SSO_OP_OBJ(lang_case_discriminator),\
    SSO_OP_OBJ(lang_case_type),\
    SSO_OP_OBJ(lang_case_modifiers),\
    /* default */\
    SSO_OP_OBJ(lang_default_type),\
    /* parameter */\
    SSO_OP_OBJ(lang_parameter_name),\
    SSO_OP_OBJ(lang_parameter_type),\
    SSO_OP_OBJ(lang_parameter_inout),\
    SSO_OP_OBJ(lang_parameter_passByReference),\
    /* sample */\
    SSO_OP_OBJ(vstore_sample_timestamp),\
    SSO_OP_OBJ(vstore_sample_value),\
    /* result */\
    SSO_OP_OBJ(vstore_result_id),\
    SSO_OP_OBJ(vstore_result_name),\
    SSO_OP_OBJ(vstore_result_parent),\
    SSO_OP_OBJ(vstore_result_type),\
    SSO_OP_OBJ(vstore_result_value),\
    SSO_OP_OBJ(vstore_result_flags),\
    SSO_OP_OBJ(vstore_result_object),\
    SSO_OP_OBJ(vstore_result_history),\
    SSO_OP_OBJ(vstore_result_owner),\
    SSO_OP_OBJ(vstore_result_getText_),\
    SSO_OP_OBJ(vstore_result_fromcontent_),\
    SSO_OP_OBJ(vstore_result_contentof_),\
    /* package */\
    SSO_OP_OBJ(lang_package_description),\
    SSO_OP_OBJ(lang_package_version),\
    SSO_OP_OBJ(lang_package_author),\
    SSO_OP_OBJ(lang_package_organization),\
    SSO_OP_OBJ(lang_package_url),\
    SSO_OP_OBJ(lang_package_repository),\
    SSO_OP_OBJ(lang_package_license),\
    SSO_OP_OBJ(lang_package_icon),\
    SSO_OP_OBJ(lang_package_use),\
    SSO_OP_OBJ(lang_package_public),\
    SSO_OP_OBJ(lang_package_managed),\
    SSO_OP_OBJ(lang_package_init_),\
    SSO_OP_OBJ(lang_package_construct_),\
    /* time */\
    SSO_OP_OBJ(vstore_time_sec),\
    SSO_OP_OBJ(vstore_time_nanosec),\
    /* frame */\
    SSO_OP_OBJ(vstore_frame_kind),\
    SSO_OP_OBJ(vstore_frame_value),\
    SSO_OP_OBJ(vstore_frame_getTime_),\
    /* native/type */\
    SSO_OP_OBJ(native_type_name),\
    SSO_OP_OBJ(native_type_init_),\
    /* secure/key */\
    SSO_OP_OBJ(secure_key_construct_),\
    SSO_OP_OBJ(secure_key_destruct_),\
    SSO_OP_OBJ(secure_key_authenticate_),\
    /* secure/lock */\
    SSO_OP_OBJ(secure_lock_mount),\
    SSO_OP_OBJ(secure_lock_expr),\
    SSO_OP_OBJ(secure_lock_priority),\
    SSO_OP_OBJ(secure_lock_construct_),\
    SSO_OP_OBJ(secure_lock_destruct_),\
    SSO_OP_OBJ(secure_lock_authorize_)\


typedef struct corto_bootstrapElement {
    corto_object o;
    size_t size;
} corto_bootstrapElement;

corto_bootstrapElement types[] = {
    SSO_OP_TYPE(),
    {NULL, 0}
};

corto_bootstrapElement objects[] = {
    SSO_OP_OBJECT(),
    {NULL, 0}
};

/* Initialization of objects */
static
void corto_initObject(
    corto_object o)
{
    corto_init_builtin(o);
    corto_walk_opt s =
        corto_ser_init(0, CORTO_NOT, CORTO_WALK_TRACE_ON_FAIL);
    corto_walk(&s, o, NULL);
    corto_type t = corto_typeof(o);
    corto_invoke_preDelegate(&t->init, t, o);
}

/* Define object */
static
void corto_defineObject(
    corto_object o)
{
    if (corto_define(o)) {
        corto_throw("construction of builtin-object '%s' failed", corto_idof(o));
    }
}

/* Define type */
static
void corto_defineType(
    corto_object o,
    corto_uint32 size)
{
    corto_defineObject(o);

    if (corto_type(o)->size != size) {
        corto_error(
          "bootstrap: size validation failed for type '%s' - metatype = %d, c-type = %d.",
          corto_fullpath(NULL, o), corto_type(o)->size, size);
    }
}

static
void corto_genericTlsFree(
    void *o)
{
    corto_dealloc(o);
}

static
void corto_patchSequences(void) {
    /* Mount implements dispatcher */
    corto_mount_o->implements.length = 1;
    corto_mount_o->implements.buffer = corto_alloc(sizeof(corto_object));
    corto_mount_o->implements.buffer[0] = corto_dispatcher_o;

    /* Add parameter to handleAction */
    corto_handleAction_o->parameters.length = 1;
    corto_handleAction_o->parameters.buffer = corto_calloc(sizeof(corto_parameter));
    corto_parameter *p = &corto_handleAction_o->parameters.buffer[0];
    corto_set_ref(&p->type, corto_event_o);
    corto_set_str(&p->name, "event");
}

static
void corto_environment_init(void)
{
/* Only set environment variables if library is installed as corto package */
#ifndef CORTO_STANDALONE_LIB

    /* BAKE_HOME is where corto binaries are located */
    if (!corto_getenv("BAKE_HOME") || !strlen(corto_getenv("BAKE_HOME"))) {
        corto_setenv("BAKE_HOME", "/usr/local");
    }

    /* If there is no home directory, default to /usr/local. This should be
     * avoided in development environments. */
    if (!corto_getenv("HOME") || !strlen(corto_getenv("HOME"))) {
        corto_setenv("HOME", "/usr/local");
    }

    /* BAKE_TARGET is where a project will be built */
    if (!corto_getenv("BAKE_TARGET") || !strlen(corto_getenv("BAKE_TARGET"))) {
        corto_setenv("BAKE_TARGET", "~/.corto");
    }

    /* BAKE_VERSION points to the current major-minor version */
    if (!corto_getenv("BAKE_VERSION")) {
        corto_setenv("BAKE_VERSION", VERSION_MAJOR "." VERSION_MINOR);
    }
#endif

    corto_string traceObject = corto_getenv("CORTO_TRACE_ID");
    if (traceObject && traceObject[0]) {
        CORTO_TRACE_ID = traceObject;
    }

    corto_string enableBacktrace = corto_getenv("CORTO_BACKTRACE_ENABLED");
    if (enableBacktrace) {
        CORTO_BACKTRACE_ENABLED = !strcmp(enableBacktrace, "true");
    }

    corto_string memtraceBreakpoint = corto_getenv("CORTO_MEMTRACE_BREAKPOINT");
    if (memtraceBreakpoint && memtraceBreakpoint[0]) {
        CORTO_MEMTRACE_BREAKPOINT = atoi(memtraceBreakpoint);
    }

    corto_string errfmt = corto_getenv("CORTO_LOGFMT");
    if (errfmt && errfmt[0]) {
        corto_log_fmt(errfmt);
    }
}

int corto_load_config(void)
{
    int result = 0;
    corto_debug("load configuration");
    corto_log_push("config");
    char *cfg = corto_getenv("CORTO_CONFIG");
    if (cfg) {
        if (corto_isdir(cfg)) {
            corto_trace("loading configuration");
            char *prevDir = strdup(corto_cwd());
            corto_ll cfgfiles = corto_opendir(cfg);
            corto_chdir(cfg);
            corto_iter it = corto_ll_iter(cfgfiles);
            while (corto_iter_hasNext(&it)) {
                char *file = corto_iter_next(&it);
                if (corto_use(file, 0, NULL)) {
                    corto_raise();
                    result = -1;
                    /* Don't break, report all errors */
                } else {
                    corto_ok("successfuly loaded '%s'", file);
                }
            }
            corto_chdir(prevDir);
            corto_closedir(cfgfiles);
        } else if (corto_file_test(cfg)) {
            if (corto_use(cfg, 0, NULL)) {
                result = -1;
            }
        } else {
            corto_trace(
                "$CORTO_CONFIG ('%s') does not point to an accessible "
                "path or file",
                cfg);
        }
    }
    corto_log_pop();

    return result;
}

/* Bootstrap corto object store */
int corto_start(
    char *appName)
{
    CORTO_APP_STATUS = 1; /* Initializing */

    corto_appName = appName;
    if ((appName[0] == '.') && (appName[1] == '/')) {
        corto_appName += 2;
    }

    platform_init(appName);

    /* Initialize TLS keys */
    corto_tls_new(&CORTO_KEY_OBSERVER_ADMIN, corto_observerAdminFree);
    corto_tls_new(&CORTO_KEY_DECLARED_ADMIN, corto_declaredByMeFree);
    corto_tls_new(&CORTO_KEY_LISTEN_ADMIN, NULL);
    corto_tls_new(&CORTO_KEY_OWNER, NULL);
    corto_tls_new(&CORTO_KEY_ATTR, corto_genericTlsFree);
    corto_tls_new(&CORTO_KEY_FLUENT, NULL);
    corto_tls_new(&CORTO_KEY_MOUNT_RESULT, NULL);
    corto_tls_new(&CORTO_KEY_CONSTRUCTOR_TYPE, NULL);
    corto_tls_new(&corto_subscriber_admin.key, corto_entityAdmin_free);
    corto_tls_new(&corto_mount_admin.key, corto_entityAdmin_free);

    /* Initialize operating system environment */
    corto_environment_init();

    /* Push init component for logging */
    corto_log_push("init");

    corto_trace("initializing...");

    /* Initialize loader */
    corto_load_init(
        corto_getenv("BAKE_TARGET"),
        corto_getenv("BAKE_HOME"),
        corto_getenv("BAKE_VERSION"),
        corto_get_build());

    /* Initialize security */
    corto_debug("init security");
    corto_secure_init();

    /* Register CDECL as first binding */
    corto_debug("init C binding");
    if (corto_invoke_register(corto_cdeclInit, corto_cdeclDeinit) != CORTO_PROCEDURE_CDECL) {
        /* Sanity check */
        corto_critical("CDECL binding did not register with id 1");
    }

    corto_debug("init global administration");

    /* Init admin-lock */
    corto_mutex_new(&corto_adminLock);
    corto_rwmutex_new(&corto_subscriberLock);

    /* Bootstrap sizes of types used in parameters, these are used to determine
     * argument-stack sizes for functions during function::construct. */
    corto_type(corto_string_o)->size = sizeof(corto_string);
    corto_type(corto_int32_o)->size = sizeof(corto_int32);
    corto_type(corto_uint32_o)->size = sizeof(corto_uint32);
    corto_type(corto_any_o)->size = sizeof(corto_any);
    corto_type(corto_state_o)->size = sizeof(corto_state);
    corto_type(corto_attr_o)->size = sizeof(corto_attr);

    /* Bootstrap offsets of delegates. These are required to pull forward
     * delegates from base classes */
    lang_type_init__o.v.offset = offsetof(struct corto_type_s, init);
    lang_type_deinit__o.v.offset = offsetof(struct corto_type_s, deinit);
    lang_class_construct__o.v.offset = offsetof(struct corto_class_s, construct);
    lang_class_define__o.v.offset = offsetof(struct corto_class_s, define);
    lang_class_destruct__o.v.offset = offsetof(struct corto_class_s, destruct);
    lang_class_delete__o.v.offset = offsetof(struct corto_class_s, _delete);
    lang_class_update__o.v.offset = offsetof(struct corto_class_s, update);
    lang_class_validate__o.v.offset = offsetof(struct corto_class_s, validate);

    /* Assign delegates to global variables, so they can be used by other code
     * without having to do expensive lookups */
    corto_type_init_o = &lang_type_init__o.v;
    corto_type_deinit_o = &lang_type_deinit__o.v;
    corto_class_construct_o = &lang_class_construct__o.v;
    corto_class_define_o = &lang_class_define__o.v;
    corto_class_destruct_o = &lang_class_destruct__o.v;
    corto_class_delete_o = &lang_class_delete__o.v;
    corto_class_validate_o = &lang_class_validate__o.v;
    corto_class_update_o = &lang_class_update__o.v;

    /* Set version of builtin packages */
    corto_o->version = (char*)VERSION_MAJOR "." VERSION_MINOR "." VERSION_PATCH;
    corto_lang_o->version = (char*)VERSION_MAJOR "." VERSION_MINOR "." VERSION_PATCH;
    corto_vstore_o->version = (char*)VERSION_MAJOR "." VERSION_MINOR "." VERSION_PATCH;
    corto_secure_o->version = (char*)VERSION_MAJOR "." VERSION_MINOR "." VERSION_PATCH;
    corto_native_o->version = (char*)VERSION_MAJOR "." VERSION_MINOR "." VERSION_PATCH;

    /* Initialize builtin scopes */
    corto_debug("init builtin packages");
    corto_initObject(root_o);
    corto_initObject(corto_o);
    corto_initObject(corto_lang_o);
    corto_initObject(corto_vstore_o);
    corto_initObject(corto_native_o);
    corto_initObject(corto_secure_o);

    /* Allocate lists */
    corto_o->use = corto_ll_new();
    corto_lang_o->use = corto_ll_new();
    corto_vstore_o->use = corto_ll_new();
    corto_native_o->use = corto_ll_new();
    corto_secure_o->use = corto_ll_new();

    /* Define builtin scopes */
    corto_defineObject(root_o);
    corto_defineObject(corto_o);
    corto_defineObject(corto_lang_o);
    corto_defineObject(corto_vstore_o);
    corto_defineObject(corto_native_o);
    corto_defineObject(corto_secure_o);

    /* Because at this point the construct/destruct & init/deinit delegates are
     * not yet propagated from base classes to sub classes, type construction
     * can't begin yet. First, delegates need to be properly initialized for
     * all types. This only affects types that support inheritance, so all
     * types that inherit from interface. */

    corto_debug("init delegates");
    corto_int32 i = 0;
    corto_object o;
    for (i = 0; (o = types[i].o); i++) {
        if (corto_instanceof(corto_interface_o, o)) {
            if (corto_interface_pullDelegate(o, &lang_type_init__o.v)) {
                ((corto_type)o)->flags |= CORTO_TYPE_HAS_INIT;
            }
            if (corto_interface_pullDelegate(o, &lang_type_deinit__o.v)) {
                ((corto_type)o)->flags |= CORTO_TYPE_HAS_DEINIT;
            }
            if (corto_instanceof(corto_class_o, o)) {
                if (corto_interface_pullDelegate(o, &lang_class_construct__o.v)) {
                    ((corto_type)o)->flags |= CORTO_TYPE_HAS_CONSTRUCT;
                }
                if (corto_interface_pullDelegate(o, &lang_class_destruct__o.v)) {
                    ((corto_type)o)->flags |= CORTO_TYPE_HAS_DESTRUCT;
                }

                /* Other delegates are not relevant for construction of types */
            }
        }
    }

    /* Init objects */
    corto_debug("init builtin objects");
    for (i = 0; (o = types[i].o); i++) corto_initObject(o);
    for (i = 0; (o = objects[i].o); i++) corto_initObject(o);

    /* Patch sequences- these aren't set statically since sequences are
     * allocated on the heap */
    corto_patchSequences();

    /* Mark the tableinstance type as a container. Even though it does not
     * inherit from the container base class, object management should treat it
     * as one (automatically defining / declaring child objects) */
    ((corto_type)corto_tableinstance_o)->flags |= CORTO_TYPE_IS_CONTAINER;

    /* Manually assign two function objects that are used as delegate callbacks */
    corto_observer_event_handle_o = &vstore_observer_event_handle__o.v;
    corto_subscriber_event_handle_o = &vstore_subscriber_event_handle__o.v;

    /* Construct objects */
    for (i = 0; (o = objects[i].o); i++) corto_defineObject(o);
    for (i = 0; (o = types[i].o); i++) corto_defineType(o, types[i].size);

    /* Initialize conversions and operators */
#ifdef CORTO_CONVERSIONS
    corto_ptr_castInit();
#endif
#ifdef CORTO_OPERATORS
    corto_ptr_operatorInit();
#endif

    /* Register exit-handler */
    void corto_loaderOnExit(void* ctx);
    corto_onexit(corto_loaderOnExit, NULL);

    /* Register library-binding */
    corto_debug("init builtin file extensions");

    int corto_load_libraryAction(corto_string file, int argc, char* argv[], void *data);
    corto_load_register("so", corto_load_libraryAction, NULL);

    int corto_file_loader(corto_string file, int argc, char* argv[], void *data);
    corto_load_register("", corto_file_loader, NULL);

    /* Always randomize seed */
    srand (time(NULL));

    CORTO_APP_STATUS = 0; /* Running */

    /* Create builtin root scopes */
    corto_debug("init root scopes");

    corto_object config_o = corto(CORTO_DECLARE|CORTO_DEFINE|CORTO_FORCE_TYPE,
        {.parent = root_o, .id = "config", .type = corto_void_o});

    corto(CORTO_DECLARE|CORTO_DEFINE|CORTO_FORCE_TYPE,
        {.parent = root_o, .id = "data", .type = corto_void_o});

    corto(CORTO_DECLARE|CORTO_DEFINE|CORTO_FORCE_TYPE,
        {.parent = root_o, .id = "home", .type = corto_void_o});

/* Only create package mount for non-redistributable version of corto, where
 * packages are installed in a common location */
#ifndef CORTO_STANDALONE_LIB
    corto_debug("init package loader");

    corto_loaderInstance = corto(CORTO_DECLARE|CORTO_DEFINE|CORTO_FORCE_TYPE,
        {.parent = config_o, .id = "loader", .type = corto_loader_o});

    if (corto_loaderInstance) {
        corto_loaderInstance->autoLoad = TRUE;
    } else {
        corto_raise();
        corto_trace("autoloading of packages disabled");
    }
#endif

    /* Pop init log component */
    corto_log_pop();

    return 0;
}

/* Register exithandler */
void corto_onexit(
    void(*handler)(void*),
    void* userData)
{
    struct corto_exitHandler* h;

    h = corto_alloc(sizeof(struct corto_exitHandler));
    h->handler = handler;
    h->userData = userData;

    corto_mutex_lock(&corto_adminLock);
    if (!corto_exitHandlers) {
        corto_exitHandlers = corto_ll_new();
    }
    corto_ll_insert(corto_exitHandlers, h);
    corto_mutex_unlock(&corto_adminLock);
}

/* Call exit-handlers */
static
void corto_exit(void)
{
    struct corto_exitHandler* h;

    if (corto_exitHandlers) {
        while((h = corto_ll_takeFirst(corto_exitHandlers))) {
            h->handler(h->userData);
            corto_dealloc(h);
        }
        corto_ll_free(corto_exitHandlers);
        corto_exitHandlers = NULL;
    }
}

/* Shutdown object store */
int corto_stop(void)
{
    CORTO_APP_STATUS = 2; /* Shutting down */

    corto_log_push("fini");

    corto_trace("shutting down...");

    if (corto_get_source()) {
        corto_error("owner has not been reset to NULL before shutting down");
        abort();
    }

#ifndef CORTO_STANDALONE_LIB
    if (corto_loaderInstance) {
        corto_debug("cleanup package loader");
        corto_delete(corto_loaderInstance);
    }
#endif

    /* Drop the rootscope. This will not actually result
     * in removing the rootscope itself, but it will result in the
     * removal of all non-static objects. */
    corto_debug("cleanup objects");
    corto_drop(root_o, FALSE);

    corto_int32 i;
    corto_object o;

    /* Destruct objects */
    corto_debug("cleanup builtin objects");
    for (i = 0; (o = types[i].o); i++) corto_destruct(o, FALSE);
    for (i = 0; (o = objects[i].o); i++) corto_destruct(o, FALSE);

    /* Free objects */
    for (i = 0; (o = objects[i].o); i++) corto_deinit_builtin(o);
    for (i = 0; (o = types[i].o); i++) corto_deinit_builtin(o);

    if (corto_deinit_builtin(corto_native_o)) goto error;
    if (corto_deinit_builtin(corto_vstore_o)) goto error;
    if (corto_deinit_builtin(corto_lang_o)) goto error;
    if (corto_deinit_builtin(corto_o)) goto error;
    if (corto_deinit_builtin(root_o)) goto error;

    /* Deinit adminLock */
    corto_debug("cleanup global administration");
    corto_mutex_free(&corto_adminLock);
    corto_rwmutex_free(&corto_subscriberLock);

    corto_log_pop();

    platform_deinit();

    /* Call exithandlers. Do after platform_init as this will unload any loaded
     * libraries, which may have routines to cleanup TLS data. */
    corto_exit();

    CORTO_APP_STATUS = 3; /* Shut down */

    return 0;
error:
    return -1;
}

/* Get current build. Used to check if packages link with correct corto lib */
corto_string corto_get_build(void) {
    return CORTO_BUILD;
}

/* Enable or disable autoloading of packages when a package object is created */
bool corto_autoload(bool autoload)
{
    bool prev = false;

    if (corto_loaderInstance) {
        prev = corto_loaderInstance->autoLoad;
        corto_loaderInstance->autoLoad = autoload;
    }

    return prev;
}

/* Enable or disable package loader mount. */
corto_bool corto_enableload(corto_bool enable)
{
    corto_bool prev = FALSE;
    if (!enable) {
        if (corto_loaderInstance) {
            corto_delete(corto_loaderInstance);
            corto_loaderInstance = NULL;
            prev = TRUE;
        }
    } else {
        if (!corto_loaderInstance) {
            corto_loaderInstance = corto_create(
                root_o, "config/loader", corto_loader_o);
        } else {
            prev = TRUE;
        }
    }

    return prev;
}

/* Assert object is of specified type. Used in generated type macro's. */
corto_object _corto_assert_type(
    corto_type type,
    corto_object o)
{
    corto_assert_object(type);
    corto_assert_object(o);

    if (o && (corto_typeof(o) != type)) {
        if (!_corto_instanceof(type, o)) {
            corto_error("object '%s' is not an instance of '%s'\n   type = %s",
                corto_fullpath(NULL, o),
                corto_fullpath(NULL, type),
                corto_fullpath(NULL, corto_typeof(o)));
            corto_backtrace(stdout);
            abort();
        }
    }
    return o;
}

/* Assert object is valid. Only enabled in debug builds */
#ifndef NDEBUG
void _corto_assert_object(char const *file, unsigned int line, corto_object o) {
    if (o) {
        corto__object *_o = corto_hdr(o);
        if (_o->magic != CORTO_MAGIC) {
            if (_o->magic == CORTO_MAGIC_DESTRUCT) {
                corto_critical_fl(file, line, "address <%p> points to an object that is already deleted", o);
            } else {
                corto_critical_fl(file, line, "address <%p> does not point to an object", o);
            }
        }
    }
}
#endif
