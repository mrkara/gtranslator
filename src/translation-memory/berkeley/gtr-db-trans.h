/*
 * Copyright (C) 2008  Ignacio Casal Quinteiro <nacho.resa@gmail.com>
 * 
 *     This program is free software: you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation, either version 3 of the License, or
 *     (at your option) any later version.
 * 
 *     This program is distributed in the hope that it will be useful,
 *     but WITHOUT ANY WARRANTY; without even the implied warranty of
 *     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *     GNU General Public License for more details.
 * 
 *     You should have received a copy of the GNU General Public License
 *     along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __DB_TRANS_H__
#define __DB_TRANS_H__

#include <glib.h>
#include <glib-object.h>
#include <gtk/gtk.h>
#include "gtr-db-base.h"

G_BEGIN_DECLS
/*
 * Type checking and casting macros
 */
#define GTR_TYPE_DB_TRANS		(gtr_db_trans_get_type ())
#define GTR_DB_TRANS(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), GTR_TYPE_DB_TRANS, GtrDbTrans))
#define GTR_DB_TRANS_CLASS(k)	(G_TYPE_CHECK_CLASS_CAST((k), GTR_TYPE_DB_TRANS, GtrDbTransClass))
#define GTR_IS_DB_TRANS(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), GTR_TYPE_DB_TRANS))
#define GTR_IS_DB_TRANS_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), GTR_TYPE_DB_TRANS))
#define GTR_DB_TRANS_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), GTR_TYPE_DB_TRANS, GtrDbTransClass))
/* Private structure type */
typedef struct _GtrDbTransPrivate GtrDbTransPrivate;

/*
 * Main object structure
 */
typedef struct _GtrDbTrans GtrDbTrans;

struct _GtrDbTrans
{
  GtrDbBase parent_instance;

  /*< private > */
  GtrDbTransPrivate *priv;
};

/*
 * Class definition
 */
typedef struct _GtrDbTransClass GtrDbTransClass;

struct _GtrDbTransClass
{
  GtrDbBaseClass parent_class;
};

/*
 * Public methods
 */
GType
gtr_db_trans_get_type (void)
  G_GNUC_CONST;

     GType gtr_db_trans_register_type (GTypeModule * module);

     GtrDbTrans *gtr_db_trans_new (void);

     db_recno_t gtr_db_trans_write_string (GtrDbTrans *
                                           db_trans,
                                           const gchar * translation,
                                           db_recno_t key);

     db_recno_t gtr_db_trans_write (GtrDbTrans * db_trans,
                                    gchar ** translations, db_recno_t index);

     GPtrArray *gtr_db_trans_read (GtrDbTrans * db_trans, db_recno_t index);

G_END_DECLS
#endif /* __DB_TRANS_H__ */