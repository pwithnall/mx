/*
 * mx-item-view.h: MxGrid powered by a model
 *
 * Copyright 2009 Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU Lesser General Public License,
 * version 2.1, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for
 * more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 * Boston, MA 02111-1307, USA.
 *
 * Written by: Thomas Wood <thomas.wood@intel.com>
 *
 */

#if !defined(MX_H_INSIDE) && !defined(MX_COMPILATION)
#error "Only <mx/mx.h> can be included directly.h"
#endif

#ifndef _MX_LIST_VIEW_H
#define _MX_LIST_VIEW_H

#include <glib-object.h>
#include "mx-grid.h"
#include "mx-item-factory.h"

G_BEGIN_DECLS

#define MX_TYPE_LIST_VIEW mx_list_view_get_type()

#define MX_LIST_VIEW(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
  MX_TYPE_LIST_VIEW, MxListView))

#define MX_LIST_VIEW_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), \
  MX_TYPE_LIST_VIEW, MxListViewClass))

#define MX_IS_LIST_VIEW(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
  MX_TYPE_LIST_VIEW))

#define MX_IS_LIST_VIEW_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), \
  MX_TYPE_LIST_VIEW))

#define MX_LIST_VIEW_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), \
  MX_TYPE_LIST_VIEW, MxListViewClass))

typedef struct _MxListViewPrivate MxListViewPrivate;

/**
 * MxListView:
 *
 * The contents of the this structure are private and should only be accessed
 * through the public API.
 */
typedef struct {
  /*< private >*/
  MxGrid parent;

  MxListViewPrivate *priv;
} MxListView;

typedef struct {
  MxGridClass parent_class;
} MxListViewClass;

GType mx_list_view_get_type (void);

ClutterActor *mx_list_view_new (void);

void          mx_list_view_set_model     (MxListView    *list_view,
                                          ClutterModel  *model);
ClutterModel* mx_list_view_get_model     (MxListView    *list_view);

void          mx_list_view_set_item_type (MxListView    *list_view,
                                          GType          item_type);
GType         mx_list_view_get_item_type (MxListView    *list_view);

void          mx_list_view_add_attribute (MxListView    *list_view,
                                          const gchar   *attribute,
                                          gint           column);

void          mx_list_view_freeze        (MxListView    *list_view);
void          mx_list_view_thaw          (MxListView    *list_view);
void          mx_list_view_set_factory   (MxListView    *list_view,
                                          MxItemFactory *factory);

G_END_DECLS

#endif /* _MX_LIST_VIEW_H */
