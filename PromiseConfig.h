#ifndef PROMISE_CONFIG_H
#define PROMISE_CONFIG_H

#ifndef USE_DISPATCH
# define USE_DISPATCH 0
#endif // USE_DISPATCH

#ifndef DEFERRED_PROMISE
# define DEFERRED_PROMISE 1
#endif // DEFERRED_PROMISE

#ifndef NESTING_PROMISE
# define NESTING_PROMISE 0
#endif // NESTING_PROMISE

/*
 * `onReject` resolved with default constructed type T if no `Promise<t>` explicitly provided
 */
#ifndef ONREJECT_IMPLICITLY_RESOLVED
# define ONREJECT_IMPLICITLY_RESOLVED 1
#endif // ONREJECT_IMPLICITLY_RESOLVED


#endif // PROMISE_CONFIG_H
