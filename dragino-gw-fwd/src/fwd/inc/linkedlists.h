/*
 *  ____  ____      _    ____ ___ _   _  ___  
 *  |  _ \|  _ \    / \  / ___|_ _| \ | |/ _ \ 
 *  | | | | |_) |  / _ \| |  _ | ||  \| | | | |
 *  | |_| |  _ <  / ___ \ |_| || || |\  | |_| |
 *  |____/|_| \_\/_/   \_\____|___|_| \_|\___/ 
 *
 * Dragino_gw_fwd -- An opensource lora gateway forward 
 *
 * See http://www.dragino.com for more information about
 * the lora gateway project. Please do not directly contact
 * any of the maintainers of this project for assistance;
 *
 * This program is free software, distributed under the terms of
 * the GNU General Public License Version 2. See the LICENSE file
 * at the top of the source tree.
 *
 * Maintainer: skerlan
 *
 */

/*!
 * \file
 * \brief Persistant data storage 
 */

#ifndef LGW_LINKEDLISTS_H
#define LGW_LINKEDLISTS_H

/*!
 * \file linkedlists.h
 * \brief A set of macros to manage forward-linked lists.
 */

/*!
 * \brief Locks a list.
 * \param head This is a pointer to the list head structure
 *
 * This macro attempts to place an exclusive lock in the
 * list head structure pointed to by head.
 * \retval 0 on success
 * \retval non-zero on failure
 */
#define LGW_LIST_LOCK(head)						\
	pthread_mutex_lock(&(head)->lock)

/*!
 * \brief Locks a list, without blocking if the list is locked.
 * \param head This is a pointer to the list head structure
 *
 * This macro attempts to place an exclusive lock in the
 * list head structure pointed to by head.
 * \retval 0 on success
 * \retval non-zero on failure
 */
#define LGW_LIST_TRYLOCK(head)						\
	pthread_mutex_trylock(&(head)->lock)

/*!
 * \brief Attempts to unlock a list.
 * \param head This is a pointer to the list head structure
 *
 * This macro attempts to remove an exclusive lock from the
 * list head structure pointed to by head. If the list
 * was not locked by this thread, this macro has no effect.
 */
#define LGW_LIST_UNLOCK(head)						\
	pthread_mutex_unlock(&(head)->lock)

/*!
 * \brief Defines a structure to be used to hold a list of specified type.
 * \param name This will be the name of the defined structure.
 * \param type This is the type of each list entry.
 *
 * This macro creates a structure definition that can be used
 * to hold a list of the entries of type \a type. It does not actually
 * declare (allocate) a structure; to do that, either follow this
 * macro with the desired name of the instance you wish to declare,
 * or use the specified \a name to declare instances elsewhere.
 *
 * Example usage:
 * \code
 * static LGW_LIST_HEAD(entry_list, entry) entries;
 * \endcode
 *
 * This would define \c struct \c entry_list, and declare an instance of it named
 * \a entries, all intended to hold a list of type \c struct \c entry.
 */
#define LGW_LIST_HEAD(name, type)			\
struct name {								\
	struct type *first;						\
	struct type *last;						\
    int         size;                       \
	pthread_mutex_t lock;					\
}

/*!
 * \brief Defines a structure to be used to hold a list of specified type (with no lock).
 * \param name This will be the name of the defined structure.
 * \param type This is the type of each list entry.
 *
 * This macro creates a structure definition that can be used
 * to hold a list of the entries of type \a type. It does not actually
 * declare (allocate) a structure; to do that, either follow this
 * macro with the desired name of the instance you wish to declare,
 * or use the specified \a name to declare instances elsewhere.
 *
 * Example usage:
 * \code
 * static LGW_LIST_HEAD_NOLOCK(entry_list, entry) entries;
 * \endcode
 *
 * This would define \c struct \c entry_list, and declare an instance of it named
 * \a entries, all intended to hold a list of type \c struct \c entry.
 */
#define LGW_LIST_HEAD_NOLOCK(name, type)				\
struct name {								\
	struct type *first;						\
	struct type *last;						\
    int         size;                       \
}

/*!
 * \brief Defines initial values for a declaration of LGW_LIST_HEAD
 */
#define LGW_LIST_HEAD_INIT_VALUE	{		\
	.first = NULL,					        \
	.last = NULL,					        \
    .size = 0,                              \
	.lock = PTHREAD_MUTEX_INITIALIZER,		\
	}

/*!
 * \brief Defines initial values for a declaration of LGW_LIST_HEAD_NOLOCK
 */
#define LGW_LIST_HEAD_NOLOCK_INIT_VALUE	{	\
	.first = NULL,					\
	.last = NULL,					\
    .size = 0,                      \
	}


/*!
 * \brief Defines a structure to be used to hold a list of specified type, statically initialized.
 * \param name This will be the name of the defined structure.
 * \param type This is the type of each list entry.
 *
 * This macro creates a structure definition that can be used
 * to hold a list of the entries of type \a type, and allocates an instance
 * of it, initialized to be empty.
 *
 * Example usage:
 * \code
 * static LGW_LIST_HEAD_STATIC(entry_list, entry);
 * \endcode
 *
 * This would define \c struct \c entry_list, intended to hold a list of
 * type \c struct \c entry.
 */
#define LGW_LIST_HEAD_STATIC(name, type)				\
struct name {								\
	struct type *first;						\
	struct type *last;						\
    int         size;                       \
	pthread_mutex_t lock;						\
} name = LGW_LIST_HEAD_INIT_VALUE

/*!
 * \brief Defines a structure to be used to hold a list of specified type, statically initialized.
 *
 * This is the same as LGW_LIST_HEAD_STATIC, except without the lock included.
 */
#define LGW_LIST_HEAD_NOLOCK_STATIC(name, type)				\
struct name {								\
	struct type *first;						\
	struct type *last;						\
    int         size;                       \
} name = LGW_LIST_HEAD_NOLOCK_INIT_VALUE

/*!
 * \brief Initializes a list head structure with a specified first entry.
 * \param head This is a pointer to the list head structure
 * \param entry pointer to the list entry that will become the head of the list
 *
 * This macro initializes a list head structure by setting the head
 * entry to the supplied value and recreating the embedded lock.
 */
#define LGW_LIST_HEAD_SET(head, entry) do {		\
	(head)->first = (entry);					\
	(head)->last = (entry);						\
    (head)->size = 0;                           \
	pthread_mutex_init(&(head)->lock, NULL);	\
} while (0)

/*!
 * \brief Initializes a list head structure with a specified first entry.
 * \param head This is a pointer to the list head structure
 * \param entry pointer to the list entry that will become the head of the list
 *
 * This macro initializes a list head structure by setting the head
 * entry to the supplied value.
 */
#define LGW_LIST_HEAD_SET_NOLOCK(head, entry) do {			\
	(head)->first = (entry);					\
	(head)->last = (entry);						\
    (head)->size = 0;                           \
} while (0)

/*!
 * \brief Declare a forward link structure inside a list entry.
 * \param type This is the type of each list entry.
 *
 * This macro declares a structure to be used to link list entries together.
 * It must be used inside the definition of the structure named in
 * \a type, as follows:
 *
 * \code
 * struct list_entry {
      ...
      LGW_LIST_ENTRY(list_entry) list;
 * }
 * \endcode
 *
 * The field name \a list here is arbitrary, and can be anything you wish.
 */
#define LGW_LIST_ENTRY(type)						\
struct {								\
	struct type *next;						\
}

/*!
 * \brief Returns the first entry contained in a list.
 * \param head This is a pointer to the list head structure
 */
#define LGW_LIST_FIRST(head)	((head)->first)

/*!
 * \brief Returns the last entry contained in a list.
 * \param head This is a pointer to the list head structure
 */
#define LGW_LIST_LAST(head)	((head)->last)

/*!
 * \brief Returns the next entry in the list after the given entry.
 * \param elm This is a pointer to the current entry.
 * \param field This is the name of the field (declared using LGW_LIST_ENTRY())
 * used to link entries of this list together.
 */
#define LGW_LIST_NEXT(elm, field)	((elm)->field.next)

/*!
 * \brief Checks whether the specified list contains any entries.
 * \param head This is a pointer to the list head structure
 *
 * \return zero if the list has entries
 * \return non-zero if not.
 */
#define LGW_LIST_EMPTY(head)	(LGW_LIST_FIRST(head) == NULL)


/*!
 * \brief Loops over (traverses) the entries in a list.
 * \param head This is a pointer to the list head structure
 * \param var This is the name of the variable that will hold a pointer to the
 * current list entry on each iteration. It must be declared before calling
 * this macro.
 * \param field This is the name of the field (declared using LGW_LIST_ENTRY())
 * used to link entries of this list together.
 *
 * This macro is use to loop over (traverse) the entries in a list. It uses a
 * \a for loop, and supplies the enclosed code with a pointer to each list
 * entry as it loops. It is typically used as follows:
 * \code
 * static LGW_LIST_HEAD(entry_list, list_entry) entries;
 * ...
 * struct list_entry {
      ...
      LGW_LIST_ENTRY(list_entry) list;
 * }
 * ...
 * struct list_entry *current;
 * ...
 * LGW_LIST_TRAVERSE(&entries, current, list) {
     (do something with current here)
 * }
 * \endcode
 */

#define LGW_LIST_TRAVERSE(head,var,field) 				\
	for((var) = (head)->first; (var); (var) = (var)->field.next)

/*!
 * \brief Loops safely over (traverses) the entries in a list.
 * \param head This is a pointer to the list head structure
 * \param var This is the name of the variable that will hold a pointer to the
 * current list entry on each iteration. It must be declared before calling
 * this macro.
 * \param field This is the name of the field (declared using LGW_LIST_ENTRY())
 * used to link entries of this list together.
 *
 * This macro is used to safely loop over (traverse) the entries in a list. It
 * uses a \a for loop, and supplies the enclosed code with a pointer to each list
 * entry as it loops. It is typically used as follows:
 *
 * \code
 * static LGW_LIST_HEAD(entry_list, list_entry) entries;
 * ...
 * struct list_entry {
      ...
      LGW_LIST_ENTRY(list_entry) list;
 * }
 * ...
 * struct list_entry *current;
 * ...
 * LGW_LIST_TRAVERSE_SAFE_BEGIN(&entries, current, list) {
     (do something with current here)
 * }
 * LGW_LIST_TRAVERSE_SAFE_END;
 * \endcode
 *
 * It differs from LGW_LIST_TRAVERSE() in that the code inside the loop can modify
 * (or even free, after calling LGW_LIST_REMOVE_CURRENT()) the entry pointed to by
 * the \a current pointer without affecting the loop traversal.
 */
#define LGW_LIST_TRAVERSE_SAFE_BEGIN(head, var, field) {				\
	typeof((head)) __list_head = head;									\
	typeof(__list_head->first) __list_next;								\
	typeof(__list_head->first) __list_prev = NULL;						\
	typeof(__list_head->first) __list_current;							\
	for ((var) = __list_head->first,									\
		__list_current = (var),											\
		__list_next = (var) ? (var)->field.next : NULL;					\
		(var);															\
		__list_prev = __list_current,									\
		(var) = __list_next,											\
		__list_current = (var),											\
		__list_next = (var) ? (var)->field.next : NULL,					\
		(void) __list_prev /* To quiet compiler? */						\
		)

/*!
 * \brief Removes the \a current entry from a list during a traversal.
 * \param field This is the name of the field (declared using LGW_LIST_ENTRY())
 * used to link entries of this list together.
 *
 * \note This macro can \b only be used inside an LGW_LIST_TRAVERSE_SAFE_BEGIN()
 * block; it is used to unlink the current entry from the list without affecting
 * the list traversal (and without having to re-traverse the list to modify the
 * previous entry, if any).
 */
#define LGW_LIST_REMOVE_CURRENT(field) do { 							\
		__list_current->field.next = NULL;								\
		__list_current = __list_prev;									\
		if (__list_prev) {												\
			__list_prev->field.next = __list_next;						\
		} else {														\
			__list_head->first = __list_next;							\
		}																\
		if (!__list_next) {												\
			__list_head->last = __list_prev;							\
		}																\
	} while (0)


/*!
 * \brief Move the current list entry to another list.
 *
 * \note This is a silly macro.  It should be done explicitly
 * otherwise the field parameter must be the same for the two
 * lists.
 *
 * LGW_LIST_REMOVE_CURRENT(field);
 * LGW_LIST_INSERT_TAIL(newhead, var, other_field);
 */
#define LGW_LIST_MOVE_CURRENT(newhead, field) do {			\
	typeof ((newhead)->first) __extracted = __list_current;	\
	LGW_LIST_REMOVE_CURRENT(field);							\
	LGW_LIST_INSERT_TAIL((newhead), __extracted, field);	\
	} while (0)


/*!
 * \brief Removes and returns the head entry from a list.
 * \param head This is a pointer to the list head structure
 * \param field This is the name of the field (declared using LGW_LIST_ENTRY())
 * used to link entries of this list together.
 *
 * Removes the head entry from the list, and returns a pointer to it.
 * This macro is safe to call on an empty list.
 */
#define LGW_LIST_REMOVE_HEAD(head, field) ({                \
        typeof((head)->first) __cur = (head)->first;        \
        if (__cur) {                        \
            (head)->first = __cur->field.next;      \
            __cur->field.next = NULL;           \
            if ((head)->last == __cur)          \
                (head)->last = NULL;            \
        }                           \
        __cur;                          \
    })

/*!
 * \brief Removes a specific entry from a list.
 * \param head This is a pointer to the list head structure
 * \param elm This is a pointer to the entry to be removed.
 * \param field This is the name of the field (declared using LGW_LIST_ENTRY())
 * used to link entries of this list together.
 * \retval elm if elm was in the list.
 * \retval NULL if elm was not in the list or elm was NULL.
 * \warning The removed entry is \b not freed.
 */
#define LGW_LIST_REMOVE(head, elm, field)                       \
    ({                                                          \
        typeof(elm) __elm = (elm);                              \
        if (__elm) {                                            \
            if ((head)->first == __elm) {                       \
                (head)->first = __elm->field.next;              \
                __elm->field.next = NULL;                       \
                if ((head)->last == __elm) {                    \
                    (head)->last = NULL;                        \
                }                                               \
                (head)->size--;                                 \
            } else {                                            \
                typeof(elm) __prev = (head)->first;             \
                while (__prev && __prev->field.next != __elm) { \
                    __prev = __prev->field.next;                \
                }                                               \
                if (__prev) {                                   \
                    __prev->field.next = __elm->field.next;     \
                    __elm->field.next = NULL;                   \
                    if ((head)->last == __elm) {                \
                        (head)->last = __prev;                  \
                    }                                           \
                    (head)->size--;                             \
                } else {                                        \
                    __elm = NULL;                               \
                }                                               \
            }                                                   \
        }                                                       \
        __elm;                                                  \
    })

/*!
 * \brief Inserts a list entry before the current entry during a traversal.
 * \param elm This is a pointer to the entry to be inserted.
 * \param field This is the name of the field (declared using LGW_LIST_ENTRY())
 * used to link entries of this list together.
 *
 * \note This macro can \b only be used inside an LGW_LIST_TRAVERSE_SAFE_BEGIN()
 * block.
 */
#define LGW_LIST_INSERT_BEFORE_CURRENT(elm, field) do {		\
	if (__list_prev) {										\
		(elm)->field.next = __list_prev->field.next;		\
		__list_prev->field.next = elm;						\
	} else {												\
		(elm)->field.next = __list_head->first;				\
		__list_head->first = (elm);							\
	}														\
	__list_prev = (elm);									\
} while (0)


/*!
 * \brief Closes a safe loop traversal block.
 */
#define LGW_LIST_TRAVERSE_SAFE_END  }

/*!
 * \brief Initializes a list head structure.
 * \param head This is a pointer to the list head structure
 *
 * This macro initializes a list head structure by setting the head
 * entry to \a NULL (empty list) and recreating the embedded lock.
 */
#define LGW_LIST_HEAD_INIT(head) {			   \
	(head)->first = NULL;					   \
	(head)->last = NULL;					   \
    (head)->size = 0;                          \
	pthread_mutex_init(&(head)->lock, NULL);   \
}

/*!
 * \brief Destroys a list head structure.
 * \param head This is a pointer to the list head structure
 *
 * This macro destroys a list head structure by setting the head
 * entry to \a NULL (empty list) and destroying the embedded lock.
 * It does not free the structure from memory.
 */
#define LGW_LIST_HEAD_DESTROY(head) {					\
	(head)->first = NULL;						\
	(head)->last = NULL;						\
	pthread_mutex_destroy(&(head)->lock);				\
}

/*!
 * \brief Initializes a list head structure.
 * \param head This is a pointer to the list head structure
 *
 * This macro initializes a list head structure by setting the head
 * entry to \a NULL (empty list). There is no embedded lock handling
 * with this macro.
 */
#define LGW_LIST_HEAD_INIT_NOLOCK(head) {	    \
	(head)->first = NULL;						\
	(head)->last = NULL;						\
    (head)->size = 0;                           \
}

/*!
 * \brief Inserts a list entry after a given entry.
 * \param head This is a pointer to the list head structure
 * \param listelm This is a pointer to the entry after which the new entry should
 * be inserted.
 * \param elm This is a pointer to the entry to be inserted.
 * \param field This is the name of the field (declared using LGW_LIST_ENTRY())
 * used to link entries of this list together.
 */
#define LGW_LIST_INSERT_AFTER(head, listelm, elm, field) do {		\
	(elm)->field.next = (listelm)->field.next;			\
	(listelm)->field.next = (elm);					\
	if ((head)->last == (listelm))					\
		(head)->last = (elm);					\
    (head)->size++;                             \
} while (0)


/*!
 * \brief Inserts a list entry at the head of a list.
 * \param head This is a pointer to the list head structure
 * \param elm This is a pointer to the entry to be inserted.
 * \param field This is the name of the field (declared using LGW_LIST_ENTRY())
 * used to link entries of this list together.
 */
#define LGW_LIST_INSERT_HEAD(head, elm, field) do {		   \
		(elm)->field.next = (head)->first;			       \
		(head)->first = (elm);					           \
		if (!(head)->last)				           	       \
			(head)->last = (elm);				           \
        (head)->size++;                                    \
} while (0)


/*!
 * \brief Appends a list entry to the tail of a list.
 * \param head This is a pointer to the list head structure
 * \param elm This is a pointer to the entry to be appended.
 * \param field This is the name of the field (declared using LGW_LIST_ENTRY())
 * used to link entries of this list together.
 *
 * Note: The link field in the appended entry is \b not modified, so if it is
 * actually the head of a list itself, the entire list will be appended
 * temporarily (until the next LGW_LIST_INSERT_TAIL is performed).
 */
#define LGW_LIST_INSERT_TAIL(head, elm, field) do {		   \
      if (!(head)->first) {						           \
		(head)->first = (elm);					           \
		(head)->last = (elm);					           \
      } else {								               \
		(head)->last->field.next = (elm);			       \
		(head)->last = (elm);					           \
      }									                   \
     (head)->size++;                                       \
} while (0)

/*!
 * \brief Delete a element of a list.
 * \param head This is a pointer to the list head structure
 * \param elm This is a pointer to the entry to be delete.
 * \param field This is the name of the field (declared using LGW_LIST_ENTRY())
 *
 */
#define LGW_LIST_DELETE_ELM(head, elm, field) do {			\
      if (!(head)->first) {						            \
		(head)->first = NULL;					            \
		(head)->last = NULL;					            \
        (head)->size = 0;                                   \
      } else {								                \
		(head)->last->field.next = (elm)->field.next;	    \
		(head)->last = (elm);					            \
        (head)->size--;                                     \
      }									                    \
} while (0)
#endif /* _LGW_LINKEDLISTS_H */
