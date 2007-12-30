/**
 * Useful macros.
 *
 * Author:
 *   Darin Adler <darin@bentspoon.com>
 *
 * Copyright 2001 Ben Tea Spoons, Inc.
 */
#ifndef _EGG_MACROS_H_
#define _EGG_MACROS_H_

#include <glib/gmacros.h>

G_BEGIN_DECLS

/* Macros for defining classes.  Ideas taken from Nautilus and GOB. */

/* Define the boilerplate type stuff to reduce typos and code size.  Defines
 * the get_type method and the parent_class static variable. */

#define EGG_BOILERPLATE(type, type_as_function, corba_type,		\
			   parent_type, parent_type_macro,		\
			   register_type_macro)				\
static void type_as_function ## _class_init    (type ## Class *klass);	\
static void type_as_function ## _instance_init (type          *object);	\
static parent_type ## Class *parent_class = NULL;			\
static void								\
type_as_function ## _class_init_trampoline (gpointer klass,		\
					    gpointer data)		\
{									\
	parent_class = (parent_type ## Class *)g_type_class_ref (	\
		parent_type_macro);					\
	type_as_function ## _class_init ((type ## Class *)klass);	\
}									\
GType									\
type_as_function ## _get_type (void)					\
{									\
	static GType object_type = 0;					\
	if (object_type == 0) {						\
		static const GTypeInfo object_info = {			\
		    sizeof (type ## Class),				\
		    NULL,		/* base_init */			\
		    NULL,		/* base_finalize */		\
		    type_as_function ## _class_init_trampoline,		\
		    NULL,		/* class_finalize */		\
		    NULL,               /* class_data */		\
		    sizeof (type),					\
		    0,                  /* n_preallocs */		\
		    (GInstanceInitFunc) type_as_function ## _instance_init \
		};							\
		object_type = register_type_macro			\
			(type, type_as_function, corba_type,		\
			 parent_type, parent_type_macro);		\
	}								\
	return object_type;						\
}

/* Just call the parent handler.  This assumes that there is a variable
 * named parent_class that points to the (duh!) parent class.  Note that
 * this macro is not to be used with things that return something, use
 * the _WITH_DEFAULT version for that */
#define EGG_CALL_PARENT(parent_class_cast, name, args)		\
	((parent_class_cast(parent_class)->name != NULL) ?		\
	 parent_class_cast(parent_class)->name args : (void)0)

/* Same as above, but in case there is no implementation, it evaluates
 * to def_return */
#define EGG_CALL_PARENT_WITH_DEFAULT(parent_class_cast,		\
					name, args, def_return)		\
	((parent_class_cast(parent_class)->name != NULL) ?		\
	 parent_class_cast(parent_class)->name args : def_return)

/* Call a virtual method */
#define EGG_CALL_VIRTUAL(object, get_class_cast, method, args) \
    (get_class_cast (object)->method ? (* get_class_cast (object)->method) args : (void)0)

/* Call a virtual method with default */
#define EGG_CALL_VIRTUAL_WITH_DEFAULT(object, get_class_cast, method, args, default) \
    (get_class_cast (object)->method ? (* get_class_cast (object)->method) args : default)

#define EGG_CLASS_BOILERPLATE(type, type_as_function,		\
				 parent_type, parent_type_macro)	\
	EGG_BOILERPLATE(type, type_as_function, type,		\
			   parent_type, parent_type_macro,		\
			   EGG_REGISTER_TYPE)

#define EGG_REGISTER_TYPE(type, type_as_function, corba_type,		\
			    parent_type, parent_type_macro)		\
	g_type_register_static (parent_type_macro, #type, &object_info, 0)


#define EGG_DEFINE_BOXED_TYPE(TN, t_n) \
EGG_DEFINE_BOXED_TYPE_WITH_CODE(TN, t_n, {});

#define EGG_DEFINE_BOXED_TYPE_WITH_CODE(TN, t_n, _C_) \
\
static gpointer t_n##_copy (gpointer boxed); \
static void t_n##_free (gpointer boxed); \
\
EGG_DEFINE_BOXED_TYPE_EXTENDED(TN, t_n, t_n##_copy, t_n##_free, _C_);

#define EGG_DEFINE_BOXED_TYPE_EXTENDED(TN, t_n, b_c, b_f, _C_) \
\
_EGG_DEFINE_BOXED_TYPE_EXTENDED_BEGIN(TN, t_n, b_c, b_f) {_C_;} \
_EGG_DEFINE_BOXED_TYPE_EXTENDED_END()

#define _EGG_DEFINE_BOXED_TYPE_EXTENDED_BEGIN(TypeName, type_name, boxed_copy, boxed_free) \
\
GType \
type_name##_get_type (void) \
{ \
  static volatile gsize g_define_type_id__volatile = 0; \
  if (g_once_init_enter (&g_define_type_id__volatile)) \
    { \
      GType g_define_type_id = \
        g_boxed_type_register_static (g_intern_static_string (#TypeName), \
                                      boxed_copy, boxed_free); \
      { /* custom code follows */
#define _EGG_DEFINE_BOXED_TYPE_EXTENDED_END() \
        /* following custom code */ \
      }	\
      g_once_init_leave (&g_define_type_id__volatile, g_define_type_id); \
    } \
  return g_define_type_id__volatile; \
} /* closes type_name##_get_type() */

#define EGG_DEFINE_QUARK(QN, q_n) \
\
GQuark \
q_n##_quark (void) \
{ \
  static volatile gsize g_define_quark__volatile = 0; \
  if (g_once_init_enter (&g_define_quark__volatile)) \
    { \
      GQuark g_define_quark = g_quark_from_string (#QN); \
      g_once_init_leave (&g_define_quark__volatile, g_define_quark); \
    } \
  return g_define_quark__volatile; \
}

#define EGG_IS_POSITIVE_RESPONSE(response_id) \
  ((response_id) == GTK_RESPONSE_ACCEPT || \
   (response_id) == GTK_RESPONSE_OK     || \
   (response_id) == GTK_RESPONSE_YES    || \
   (response_id) == GTK_RESPONSE_APPLY)

#define EGG_IS_NEGATIVE_RESPONSE(response_id) \
  ((response_id) == GTK_RESPONSE_REJECT || \
   (response_id) == GTK_RESPONSE_CANCEL || \
   (response_id) == GTK_RESPONSE_NO)

G_END_DECLS

#endif /* _EGG_MACROS_H_ */
