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

#ifndef CORTO_VSTORE_H
#define CORTO_VSTORE_H

#include <corto/_project.h>

/* $header() */

/** @file
 * @section query Virtual Store
 * @brief API for accessing and populating the virtual store.
 *
 * The virtual store is, as the name suggests, not an actual store, but provides
 * uniform access to data from any number of 3rd party stores. Data in the
 * virtual store can move transparently to and from the in-memory store when
 * application interest emerges or goes away. This process is fully automated,
 * and does not require explicit action on behalf of the application.
 *
 * The virtual store is populated by 'mounts'. Mounts are entities which connect
 * subtrees of objects into the virtual store, which is represented to the
 * application as a single uniform tree. Operations on the virtual store are
 * translated by the "query engine" to the appropriate mounts, which will then
 * either retrieve or synchronize the requested operation from the query engine.
 *
 * Subtrees provided by mounts may be mounted in different locations by
 * different applications. The query engine automatically translates between
 * local identifiers and mount identifiers, so that a mount does not require
 * knowledge about how it connects to the virtual store.
 *
 * The mount class provides a rich interface for interacting with many kinds of
 * data. The interface is implemented as a set of overridable methods on the
 * mount class which a mount may or may not implement, based on the capabilities
 * of the underlying technology.
 *
 * A mount can reply to a single request for data, be notified
 * of subscriptions so it can provide realtime data, provide historical data,
 * synchronize data from the store, batch data from the store, downsample data
 * from the store, throttle an application (using an intelligent algorithm that
 * evenly spaces delays across sampling periods), amongst others.
 *
 * Mounts are, just like anything else, objects that are stored in the in-memory
 * store. This means that mounts can be instantiated with the regular store API,
 * and that mount configurations can be provided in any serialization format
 * that corto supports (XML, JSON, CX, ...).
 *
 * Applications can interact with the virtual store directly through the
 * `corto_select`, `corto_subscribe` and `corto_publish` APIs. Rather than
 * going through the object store, these functions communicate directly with the
 * mounts.
 *
 * Mounts use the same mechanisms to communicate with each other, which
 * means that the in-memory store can be completely bypassed. This particularly
 * speeds up scenarios where data is bridged, where two can forward directly
 * from one to another. Mounts can optionally configure a serialization format,
 * in which case corto will automatically serialize/deserialize data to the
 * correct format when it arrives/leaves the mount. When two mounts communicate
 * directly and the serialization format is the same, a quick pass-through will
 * take place.
 *
 * Mounts inherit from corto subscribers, which means that they use the same
 * mechanisms to describe data that they synchronize. Subscribers in corto use
 * a `corto_query`, which is a custom query format optimized for querying
 * hierarchies. The two most important parts of a `corto_query` are the `select`
 * and `from` fields. The `select` field specifies a filter that is matched
 * against an object id. The `from` field specifies the location in the virtual
 * store where the subscriber subscribes to. Identifiers of objects delivered to
 * the subscriber will be relative to the `from` query field.
 *
 * A mount uses the `from` field to determine where it will mount its data. This
 * also ensures that data the mount receives is relative to its mount point,
 * which allows a mount to abstract away from where it is mounted by the app.
 */

/* $end */

#include <corto/vstore/_load.h>
#include <corto/vstore/_interface.h>
#include <corto/vstore/_binding.h>

/* $body() */

#ifdef __cplusplus
extern "C" {
#endif


/* -- corto_select function -- */


typedef struct corto_select__fluent {
    /** Specify a relative scope for the query.
     * @param scope A scope identifier.
     */
    struct corto_select__fluent (*from)(
        const char *scope);

    /** Request results in a specific contentType.
     * @param contentType A MIME identifier identifying a contentType.
     */
    struct corto_select__fluent (*contentType)(
        const char *contentType);

    /** Enable pagination by specifying an object offset.
     * @param offset Specifies from which nth object results should be returned.
     */
    struct corto_select__fluent (*offset)(
        corto_uint64 offset);

    /** Enable pagination by specifying a limit to the number of objects returned.
     * @param limit Specifies the total number of results that should be returned.
     */
    struct corto_select__fluent (*limit)(
        corto_uint64 limit);

    /** Filter results by type.
     * @param filter An id expression matching one or more types.
     */
    struct corto_select__fluent (*type)(
        const char *filter);

    /** Filter out results that are not an instance of specified type.
     * @param filter An id expression matching a single type.
     */
    struct corto_select__fluent (*instanceof)(
        const char *type);

    /** Filter out results from a specific instance (mount).
     * This is typically useful when using corto_select from a mount, and the
     * mount does not want to invoke itself.
     *
     * @param instance The instance object.
     */
    struct corto_select__fluent (*instance)(
        corto_object instance);

    /** Only return results for a specific mount.
     * This is typically useful when using corto_select from a mount, and the
     * mount does not want to invoke itself.
     *
     * @param instance The instance object.
     */
    struct corto_select__fluent (*mount)(
        corto_mount mount);

    /** Request historical data starting from the current time. */
    struct corto_select__fluent (*fromNow)(void);

    /** Request historical data starting from fixed timestamp
     * @param t The timestamp from which to return historical data.
     */
    struct corto_select__fluent (*fromTime)(
        corto_time t);

    /** Request historical data until now. */
    struct corto_select__fluent (*toNow)(void);

    /** Request historical data until a fixed timestamp.
     * @param t The timestamp until which to return historical data.
     */
    struct corto_select__fluent (*toTime)(
        corto_time t);

    /** Request historical data for a specific time window.
     * @param t The duration of the time window.
     */
    struct corto_select__fluent (*forDuration)(
        corto_time t);

    /** Request historical data for a specific number of samples.
     * @param limit The number of samples per object.
     */
    struct corto_select__fluent (*slimit)(
        uint64_t limit);

    /** Request historical starting from a specific sample.
     * @param offset The offset from which to return samples.
     */
    struct corto_select__fluent (*soffset)(
        uint64_t offset);

    /** Return unknown objects.
     * An unknown object is used as a placeholder for a parent that has not yet
     * been created explicitly while one or more children of the parent have
     * been created.
     *
     * Unknown objects by themselves do not conceptually represent data, and
     * should not ordinarily be synchronized. Scenarios where an application
     * might be interested in unknown objects is where it is not guaranteed that
     * the hierarchy will be fully constructed, but applications still need to
     * be able to discover paths to where potential child objects are (like in
     * a tool that allows browsing the entire store).
     *
     * Because it is not illegal for multiple mounts to return the same
     * unknown object, disabling this filter may result in duplicate results.
     * One mount may return a normal object, while another mount may return the
     * same object identifier as unknown. Multiple unknown objects can also be
     * returned.
     */
    struct corto_select__fluent (*yield_unknown)();

    /** Return an iterator to the requested results.
     * Results are returned as corto_result instances. A corto_result contains
     * metadata and when a content type is specified, a serialized value of an
     * object. When using this function, no objects are created.
     *
     * @param iter_out A pointer to an iterator object.
     * @return 0 if success, -1 if failed.
     */
    int16_t (*iter)(
        corto_resultIter *iter_out);

    /** Resume objects into the object store.
     * This function will resume objects in the object store.
     *
     * @return 0 if success, -1 if failed.
     */
    int16_t (*resume)(void);

    /** Return an iterator to the requested objects.
     * This function will return results as anonymous objects. No objects will be
     * created in the object store.
     *
     * @param iter_out A pointer to an iterator object.
     * @return 0 if success, -1 if failed.
     */
    int16_t (*iter_objects)(
        corto_objectIter *iter_out); /* Unstable API */

    /** Return the number of objects for a query.
     * This function requires a walk over all the returned results to determine
     * the total number of objects.
     *
     * @return -1 if failed, otherwise the total number of objects.
     */
    int64_t (*count)(void);

    /* Internal APIs */
    struct corto_select__fluent (*vstore)(
        bool enable); /* Unstable API */

    int16_t (*subscribe)(corto_resultIter *ret); /* Unstable API */
    int16_t (*unsubscribe)(void); /* Unstable API */
    char* (*id)(void); /* Unstable API */
} corto_select__fluent;

/** Single-shot query.
 * With corto_select an application can select a subset of objects using a corto
 * identifier expression (parent, expr). The functionality of the corto_select
 * function is extended with fluent methods (see
 * [corto_select fluent methods](corto_select_fluent_methods)) that enable an
 * application to further narrow down results and perform various operations on
 * the objects.
 *
 * The most common operation is to request an iterator to the matching objects
 * which allows the application to iterate over the set one by one.
 *
 * The corto_select function is designed to be an API that enables accessing large
 * datasets with constrained resources. The iterative design of the API allows the
 * corto mount implementations to feed data to the application one object at a time,
 * so that even with large result sets, the memory of an application will not be
 * exhausted. Furthermore the API has native support for pagination, which allows
 * applications to further narrow down results.
 *
 * The results returned by corto_select are in abitrary order, which is a result
 * of the requirement of being able to deal with large datasets. Ordering results
 * would require obtaining a full resultset before anything can be returned to
 * the application, which is not scalable.
 *
 * The performance of corto_select highly depends on the implementation of a mount.
 * The backend provides as much information as possible upfront to the mount about
 * which information is required, which allows a mount to prefetch/cache results.
 * A mount may choose to implement such optimizations, but this is not enforced.
 *
 * The function employs minimal locking on the object store while an application
 * is iterating over a resultset. Outside of the corto_iter_next and corto_iter_hasNext
 * calls no locks will be held, which enables applications to run corto_select
 * queries concurrently and without disturbing other tasks in an application.
 *
 * @param expr An expression matching one or more objects [printf-style format specifier].
 */
CORTO_EXPORT
struct corto_select__fluent corto_select(
    const char *expr,
    ...);


/* -- corto_subscribe function -- */


typedef struct corto_subscribe__fluent {
    /** Specify a relative scope for the subscriber.
     * @param scope A scope identifier.
     */
    struct corto_subscribe__fluent (*from)(
        const char *scope);

    /** Create disabled subscriber.
     * Disabled observers allow an application to make modifications to the
     * event mask or expression, and to observe multiple expressions using
     * `corto_subscriber_subscribe`.
     */
    struct corto_subscribe__fluent (*disabled)(void);

    /** Provide dispatcher for subscriber.
     * Dispatchers intercept events before they are delivered to an subscriber
     * callback, which enables applications to implement custom event handlers.
     * A common usecase for this is to forward events to a worker thread.
     *
     * @param dispatcher A dispatcher object.
     */
    struct corto_subscribe__fluent (*dispatcher)(
        corto_dispatcher dispatcher);

    /** Specify an instance.
     * Instances are passed to subscriber callbacks. They are typically used to pass
     * the `this` variable when an subscriber is associated with a class.
     *
     * @param instance A corto object.
     */
    struct corto_subscribe__fluent (*instance)(
        corto_object instance);

    /** Request results in a specific contentType.
     * @param contentType A MIME identifier identifying a contentType.
     */
    struct corto_subscribe__fluent (*contentType)(
        const char *contentType);

    /** Filter objects by type.
     * The subscriber will only trigger on objects of the specified type.
     *
     * @param type A valid corto type identifier.
     */
    struct corto_subscribe__fluent (*type)(
        const char *type);

    /** Return unknown objects.
     * For a description of this function, see corto_select. This setting only
     * affects behavior of the subscriber during alignment, when it calls
     * corto_select.
     */
    struct corto_subscribe__fluent (*yield_unknown)(void);

    /** Create a mount of the specified type.
     *
     * @param type A mount type.
     * @param policy A mount policy.
     * @param value A corto string to set additional members of the mount.
     * @return A new mount object
     */
    corto_mount ___ (*mount)(
        corto_class type, corto_mountPolicy* policy, const char *value);

    /** Specify callback, create subscriber.
     * Provide a callback function that is invoked when a matching event occurs.
     * This function returns a new subscriber based on the specified parameters.
     *
     * @param callback An subscriber callback function.
     */
    corto_subscriber ___ (*callback)(
        void (*r)(corto_subscriber_event*));
} corto_subscribe__fluent;

/** Create a realtime query.
 * Subscribers enable an application to listen for for events from the object
 * store and events that come directly from mounts. Subscribers receive only
 * data events (`DEFINE`, `UPDATE`, `DELETE`).
 *
 * The difference between subscribers and observers is that while observers
 * provide a reference to an object, a subscriber returns a `corto_result`, which
 * contains metadata about the object, and when requested, a serialized value.
 *
 * This means that a subscriber does not require objects to be stored in the
 * in-memory object store, which makes it a better API for working with large
 * datasets.
 *
 * @param expr An expression matching one or more objects [printf-style format specifier].
 */
CORTO_EXPORT
struct corto_subscribe__fluent corto_subscribe(
    const char *expr,
    ...);

/** Delete a subscriber.
 * No more events will be delivered to the callback after this function is
 * called. Note that the subscriber object will not be deallocated if there are
 * references to the object.
 *
 * When there are still events in flight when this function is called (something
 * that can happen when a dispatcher is pushing events to another thread) the
 * application can prevent delivery of the event by checking the state of the
 * subscriber object, which is part of the `corto_subscriber_event` instance, by
 * doing:
 *
@verbatim
```c
void myDispatcher_post(corto_subscriber_event *e) {
    if (!corto_check_state(e->subscriber, CORTO_DELETED)) {
        corto_event_handle(e);
    } else {
        // Do nothing
    }
}
```
@endverbatim
 *
 * @param subscriber A subscriber object.
 * @return 0 if success, -1 if failed.
 */
CORTO_EXPORT
int16_t corto_unsubscribe(
    corto_subscriber subscriber,
    corto_object instance);

/** Publish event.
 * This function enables emitting events for objects that are not loaded in the
 * RAM store. This allows for efficient routing of events between subscribers
 * without the need to (de)marshall object values.
 *
 * If the object is loaded in the RAM store, a call to corto_publish will
 * demarshall the specified value into the object.
 *
 * The function may only emit events of the data kind, which are DEFINE,
 * UPDATE, INVALIDATE and DELETE. The other events are reserved for
 * objects that are loaded in the RAM store.
 *
 * @param event The event to be emitted
 * @param id A string representing the id of the object in the form of 'foo/bar'.
 * @param type A string representing the id of the type as returned by corto_fullpath.
 * @param contentType A string representing the content type (format) of the specified value.
 * @param value A string (or binary value) representing the serialized value of the object.
 * @return 0 if success, nonzero if failed.
 * @see corto_update_begin corto_update_end corto_update_try corto_update_cancel corto_publish
 * @see corto_observe corto_subscribe
 */
CORTO_EXPORT
int16_t corto_publish(
    corto_eventMask event,
    const char *id,
    const char *type,
    const char *contentType,
    void *content);

#ifdef __cplusplus
}
#endif

/* $end */

#endif
