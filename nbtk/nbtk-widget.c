/*
 * nbtk-widget.c: Base class for Nbtk actors
 *
 * Copyright 2007 OpenedHand
 * Copyright 2008, 2009 Intel Corporation.
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
 * Written by: Emmanuele Bassi <ebassi@openedhand.com>
 *             Thomas Wood <thomas@linux.intel.com>
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>

#include <clutter/clutter.h>

#include "nbtk-widget.h"

#include "nbtk-marshal.h"
#include "nbtk-private.h"
#include "nbtk-stylable.h"
#include "nbtk-texture-cache.h"
#include "nbtk-texture-frame.h"

/*
 * Forward declaration for sake of NbtkWidgetChild
 */
struct _NbtkWidgetPrivate
{
  NbtkPadding border;
  NbtkPadding padding;

  NbtkStyle *style;
  gchar *pseudo_class;
  gchar *style_class;

  ClutterActor *border_image;
  ClutterActor *background_image;
  ClutterColor *bg_color;

  ClutterActor *dnd_last_dest;
  gpointer      dnd_last_dest_data;
  ClutterActor *dnd_clone;
  ClutterActor *dnd_dragged;
  ClutterActor *dnd_icon;

  guint dnd_threshold;
  gint  dnd_x;
  gint  dnd_y;

  guint dnd_enter_cb_id;

  gboolean dnd_motion : 1;
  gboolean dnd_grab   : 1;
  gboolean is_stylable : 1;
};

/*
 * ClutterChildMeta Implementation
 */

enum {
  CHILD_PROP_0,

  CHILD_PROP_DND_DISABLED,
};

G_DEFINE_TYPE (NbtkWidgetChild, nbtk_widget_child, CLUTTER_TYPE_CHILD_META);

static void
widget_child_set_property (GObject      *gobject,
			   guint         prop_id,
			   const GValue *value,
			   GParamSpec   *pspec)
{
  NbtkWidgetChild *widget_child = NBTK_WIDGET_CHILD (gobject);

  switch (prop_id)
    {
    case CHILD_PROP_DND_DISABLED:
      {
	gboolean was_dnd_disabled = widget_child->dnd_disabled;

	widget_child->dnd_disabled = g_value_get_boolean (value);

	if (was_dnd_disabled != widget_child->dnd_disabled)
	  {
            ClutterChildMeta *meta = CLUTTER_CHILD_META (gobject);
	    ClutterActor *child = meta->actor;
	    NbtkWidget *widget = NBTK_WIDGET (meta->container);

	    if (was_dnd_disabled)
	      {
		if (widget->priv->dnd_threshold > 0)
                  nbtk_widget_setup_child_dnd (widget, child);
	      }
	    else
	      nbtk_widget_undo_child_dnd (widget, child);
	  }
      }
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
widget_child_get_property (GObject    *gobject,
                           guint       prop_id,
                           GValue     *value,
                           GParamSpec *pspec)
{
  NbtkWidgetChild *widget_child = NBTK_WIDGET_CHILD (gobject);

  switch (prop_id)
    {
    case CHILD_PROP_DND_DISABLED:
      g_value_set_boolean (value, widget_child->dnd_disabled);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
nbtk_widget_child_class_init (NbtkWidgetChildClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GParamSpec *pspec;

  gobject_class->set_property = widget_child_set_property;
  gobject_class->get_property = widget_child_get_property;

  pspec = g_param_spec_boolean ("dnd-disabled",
                                "DND is disabled",
                                "Indicates that this actor cannot participate "
                                "in drag and drop.",
                                FALSE,
                                NBTK_PARAM_READWRITE);
  g_object_class_install_property (gobject_class,
                                   CHILD_PROP_DND_DISABLED,
				   pspec);
}

static void
nbtk_widget_child_init (NbtkWidgetChild *self)
{
}

/**
 * SECTION:nbtk-widget
 * @short_description: Base class for stylable actors
 *
 * #NbtkWidget is a simple abstract class on top of #ClutterActor. It
 * provides basic themeing properties.
 *
 * Actors in the Nbtk library should subclass #NbtkWidget if they plan
 * to obey to a certain #NbtkStyle.
 */

enum
{
  PROP_0,

  PROP_STYLE,
  PROP_PSEUDO_CLASS,
  PROP_STYLE_CLASS,
  PROP_DND_THRESHOLD,
  PROP_DND_ICON,

  PROP_STYLABLE,
};

enum
{
  STYLE_CHANGED,
  DND_BEGIN,
  DND_MOTION,
  DND_END,
  DND_DROPPED,
  DND_ENTER,
  DND_LEAVE,

  LAST_SIGNAL
};

static guint actor_signals[LAST_SIGNAL] = { 0, };

static void nbtk_stylable_iface_init (NbtkStylableIface *iface);


G_DEFINE_ABSTRACT_TYPE_WITH_CODE (NbtkWidget, nbtk_widget, CLUTTER_TYPE_ACTOR,
                                  G_IMPLEMENT_INTERFACE (NBTK_TYPE_STYLABLE,
                                                         nbtk_stylable_iface_init));

#define NBTK_WIDGET_GET_PRIVATE(obj)    (G_TYPE_INSTANCE_GET_PRIVATE ((obj), NBTK_TYPE_WIDGET, NbtkWidgetPrivate))

static void
nbtk_widget_set_property (GObject      *gobject,
                          guint         prop_id,
                          const GValue *value,
                          GParamSpec   *pspec)
{
  NbtkWidget *actor = NBTK_WIDGET (gobject);

  switch (prop_id)
    {
    case PROP_STYLE:
      nbtk_stylable_set_style (NBTK_STYLABLE (actor),
                               g_value_get_object (value));
      break;

    case PROP_PSEUDO_CLASS:
      nbtk_widget_set_style_pseudo_class (actor, g_value_get_string (value));
      break;

    case PROP_STYLE_CLASS:
      nbtk_widget_set_style_class_name (actor, g_value_get_string (value));
      break;

    case PROP_DND_THRESHOLD:
      actor->priv->dnd_threshold = g_value_get_uint (value);
      break;

    case PROP_DND_ICON:
      actor->priv->dnd_icon = g_value_get_object (value);
      g_object_ref (actor->priv->dnd_icon);
      break;

    case PROP_STYLABLE:
      if (actor->priv->is_stylable != g_value_get_boolean (value))
        {
          actor->priv->is_stylable = g_value_get_boolean (value);
          clutter_actor_queue_relayout ((ClutterActor *)gobject);
        }
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
nbtk_widget_get_property (GObject    *gobject,
                          guint       prop_id,
                          GValue     *value,
                          GParamSpec *pspec)
{
  NbtkWidget *actor = NBTK_WIDGET (gobject);
  NbtkWidgetPrivate *priv = actor->priv;

  switch (prop_id)
    {
    case PROP_STYLE:
      g_value_set_object (value, priv->style);
      break;

    case PROP_PSEUDO_CLASS:
      g_value_set_string (value, priv->pseudo_class);
      break;

    case PROP_STYLE_CLASS:
      g_value_set_string (value, priv->style_class);
      break;

    case PROP_DND_THRESHOLD:
      g_value_set_uint (value, priv->dnd_threshold);
      break;

    case PROP_DND_ICON:
      g_value_set_object (value, priv->dnd_icon);
      break;

    case PROP_STYLABLE:
      g_value_set_boolean (value, priv->is_stylable);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
nbtk_widget_dispose (GObject *gobject)
{
  NbtkWidget *actor = NBTK_WIDGET (gobject);
  NbtkWidgetPrivate *priv = NBTK_WIDGET (actor)->priv;

  if (priv->style)
    {
      g_object_unref (priv->style);
      priv->style = NULL;
    }

  if (priv->border_image)
    {
      clutter_actor_unparent (priv->border_image);
      priv->border_image = NULL;
    }

  /*
   * If we are using custom dnd icon, release the extra reference we
   * have for it.
   *
   * NB: dnd_icon will == dnd_clone, so the extra reference should be released
   *     before we deal with dnd_clone.
   */
  if (priv->dnd_icon)
    {
      g_object_unref (priv->dnd_icon);
      priv->dnd_icon = NULL;
    }

  if (priv->dnd_clone)
    {
      ClutterActor *clone = priv->dnd_clone;
      ClutterActor *parent = clutter_actor_get_parent (clone);

      priv->dnd_clone = NULL;

      if (parent)
        {
          if (CLUTTER_IS_CONTAINER (parent))
            clutter_container_remove_actor (CLUTTER_CONTAINER (parent), clone);
          else
            clutter_actor_unparent (clone);
        }
    }

  if (priv->bg_color)
    {
      clutter_color_free (priv->bg_color);
      priv->bg_color = NULL;
    }

  if (priv->dnd_dragged)
    {
      ClutterActor *dragged = priv->dnd_dragged;
      priv->dnd_dragged = NULL;
      g_object_unref (dragged);
    }

  G_OBJECT_CLASS (nbtk_widget_parent_class)->dispose (gobject);
}

static void
nbtk_widget_finalize (GObject *gobject)
{
  NbtkWidgetPrivate *priv = NBTK_WIDGET (gobject)->priv;

  g_free (priv->style_class);

  G_OBJECT_CLASS (nbtk_widget_parent_class)->finalize (gobject);
}

static void
nbtk_widget_allocate (ClutterActor          *actor,
                      const ClutterActorBox *box,
                      gboolean               origin_changed)
{
  NbtkWidgetPrivate *priv = NBTK_WIDGET (actor)->priv;
  ClutterActorClass *klass;

  klass = CLUTTER_ACTOR_CLASS (nbtk_widget_parent_class);
  klass->allocate (actor, box, origin_changed);

  if (priv->border_image)
    {
      ClutterActorBox frame_box = {
        0,
        0,
        box->x2 - box->x1,
        box->y2 - box->y1
      };

      clutter_actor_allocate (CLUTTER_ACTOR (priv->border_image),
                              &frame_box,
                              origin_changed);
    }

  if (priv->background_image)
    {
      guint w, h;
      ClutterActorBox frame_box = {
          0, 0, box->x2 - box->x1, box->y2 - box->y1
      };

      clutter_actor_get_size (CLUTTER_ACTOR (priv->background_image), &w, &h);

      /* scale the background into the allocated bounds */
      if (w > frame_box.x2 || h > frame_box.y2)
        {
          gint new_h, new_w, offset;
          gint box_w, box_h;

          box_w = CLUTTER_UNITS_TO_INT (frame_box.x2);
          box_h = CLUTTER_UNITS_TO_INT (frame_box.y2);

          /* scale to fit */
          new_h = ((double) h / w) * ((double) box_w);
          new_w = ((double) w / h) * ((double) box_h);

          if (new_h > box_h)
            {
              /* center for new width */
              offset = ((box_w) - new_w) * 0.5;
              frame_box.x1 = offset;
              frame_box.x2 = offset + new_w;

              frame_box.y2 = box_h;
            }
          else
            {
              /* center for new height */
              offset = ((box_h) - new_h) * 0.5;
              frame_box.y1 = offset;
              frame_box.y2 = offset + new_h;

              frame_box.x2 = box_w;
            }

        }
      else
        {
          /* center the background on the widget */
          frame_box.x1 = ((box->x2 - box->x1) / 2) - (w / 2);
          frame_box.y1 = ((box->y2 - box->y1) / 2) - (h / 2);
          frame_box.x2 = frame_box.x1 + w;
          frame_box.y2 = frame_box.y1 + h;
        }

      clutter_actor_allocate (CLUTTER_ACTOR (priv->background_image),
                              &frame_box,
                              origin_changed);
    }
}

static void
nbtk_widget_real_draw_background (NbtkWidget         *self,
                                  ClutterActor       *background,
                                  const ClutterColor *color)
{
  /* Default implementation just draws the background
   * colour and the image on top
   */
  if (color && color->alpha != 0)
    {
      ClutterActor *actor = CLUTTER_ACTOR (self);
      ClutterActorBox allocation = { 0, };
      ClutterColor bg_color = *color;
      gfloat w, h;

      bg_color.alpha = clutter_actor_get_paint_opacity (actor)
                     * bg_color.alpha
                     / 255;

      clutter_actor_get_allocation_box (actor, &allocation);

      w = allocation.x2 - allocation.x1;
      h = allocation.y2 - allocation.y1;

      cogl_set_source_color4ub (bg_color.red,
                                bg_color.green,
                                bg_color.blue,
                                bg_color.alpha);
      cogl_rectangle (0, 0, w, h);
    }

  if (background)
    clutter_actor_paint (background);
}

static void
nbtk_widget_paint (ClutterActor *self)
{
  NbtkWidgetPrivate *priv = NBTK_WIDGET (self)->priv;
  NbtkWidgetClass *klass = NBTK_WIDGET_GET_CLASS (self);

  klass->draw_background (NBTK_WIDGET (self),
                          priv->border_image,
                          priv->bg_color);

  if (priv->background_image)
    clutter_actor_paint (priv->background_image);
}

static void
nbtk_widget_parent_set (ClutterActor *widget,
                        ClutterActor *old_parent)
{
  ClutterActorClass *parent_class;
  ClutterActor *new_parent;

  new_parent = clutter_actor_get_parent (widget);

  /* don't send the style changed signal if we no longer have a parent actor */
  if (new_parent)
    g_signal_emit (widget, actor_signals[STYLE_CHANGED], 0);

  parent_class = CLUTTER_ACTOR_CLASS (nbtk_widget_parent_class);
  if (parent_class->parent_set)
    parent_class->parent_set (widget, old_parent);
}

static void
nbtk_widget_style_changed (NbtkWidget *self)
{
  NbtkWidgetPrivate *priv = self->priv;
  NbtkBorderImage *border_image = NULL;
  NbtkTextureCache *texture_cache;
  ClutterActor *texture;
  gchar *bg_file;
  NbtkPadding *padding = NULL;
  gboolean relayout_needed = FALSE;

  /* application has request this widget is not stylable */
  if (!priv->is_stylable)
    return;

  if (priv->bg_color)
    {
      clutter_color_free (priv->bg_color);
      priv->bg_color = NULL;
    }

  /* cache these values for use in the paint function */
  nbtk_stylable_get (NBTK_STYLABLE (self),
                    "background-color", &priv->bg_color,
                    "background-image", &bg_file,
                    "border-image", &border_image,
                    "padding", &padding,
                    NULL);

  if (padding)
    {
      if (priv->padding.top != padding->top ||
          priv->padding.left != padding->left ||
          priv->padding.right != padding->right ||
          priv->padding.bottom != padding->bottom)
      {
        /* Padding changed. Need to relayout. */
        relayout_needed = TRUE;
      }

      priv->padding = *padding;
      g_boxed_free (NBTK_TYPE_PADDING, padding);
    }

  if (priv->border_image)
    {
       clutter_actor_unparent (CLUTTER_ACTOR (priv->border_image));
       priv->border_image = NULL;
    }
  if (priv->background_image)
    {
      clutter_actor_unparent (CLUTTER_ACTOR (priv->background_image));
      priv->background_image = NULL;
    }

  texture_cache = nbtk_texture_cache_get_default ();

  /* Check if the URL is actually present, not garbage in the property */
  if (border_image && border_image->image.uri)
    {
      gint border_left, border_right, border_top, border_bottom;

      /* `border-image' takes precedence over `background-image'.
       * Firefox lets the background-image shine thru when border-image has
       * alpha an channel, maybe that would be an option for the future. */
      texture = nbtk_texture_cache_get_texture (texture_cache,
                                                border_image->image.uri,
                                                FALSE);

      border_left = ccss_position_get_size (&border_image->left,
                                            border_image->image.width);
      border_top = ccss_position_get_size (&border_image->top,
                                           border_image->image.height);
      border_right = ccss_position_get_size (&border_image->right,
                                             border_image->image.width);
      border_bottom = ccss_position_get_size (&border_image->bottom,
                                              border_image->image.height);

      priv->border_image = nbtk_texture_frame_new (CLUTTER_TEXTURE (texture),
                                                   border_top,
                                                   border_right,
                                                   border_bottom,
                                                   border_left);
      clutter_actor_set_parent (CLUTTER_ACTOR (priv->border_image),
                                               CLUTTER_ACTOR (self));
      g_boxed_free (NBTK_TYPE_BORDER_IMAGE, border_image);
    }
  if (bg_file)
    {
      texture = nbtk_texture_cache_get_texture (texture_cache,
                                                bg_file,
                                                TRUE);
      priv->background_image = texture;

      if (!texture)
        {
          g_warning ("Could not load %s", bg_file);
        }
      else
        {
          clutter_actor_set_parent (CLUTTER_ACTOR (priv->background_image),
                                    CLUTTER_ACTOR (self));
        }
      g_free (bg_file);
    }

  /*
   * If there are any properties above that need to cause a relayout thay
   * should set this flag.
   */
  if (relayout_needed)
    clutter_actor_queue_relayout ((ClutterActor *)self);
}

static void
nbtk_widget_dnd_dropped (NbtkWidget   *actor,
			 ClutterActor *dragged,
			 ClutterActor *icon,
			 gint          x,
			 gint          y)
{
  ClutterActor *parent;

  /*
   * NbtkWidget as such does not support DND, so if the default handler is
   * called, we try to propagate the signal down the ancestry chain.
   */
  parent = clutter_actor_get_parent (CLUTTER_ACTOR (actor));
  while (parent && !NBTK_IS_WIDGET (parent))
    parent = clutter_actor_get_parent (parent);

  if (parent)
    {
      /*
       * We found a parent that is a NbtkWidget; call its handler instead.
       */
      NbtkWidgetClass *klass = NBTK_WIDGET_GET_CLASS (parent);

      if (klass->dnd_dropped)
	klass->dnd_dropped (NBTK_WIDGET (parent), dragged, icon, x, y);
    }
}

static void
nbtk_widget_class_init (NbtkWidgetClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  ClutterActorClass *actor_class = CLUTTER_ACTOR_CLASS (klass);
  GParamSpec *pspec;

  g_type_class_add_private (klass, sizeof (NbtkWidgetPrivate));

  gobject_class->set_property = nbtk_widget_set_property;
  gobject_class->get_property = nbtk_widget_get_property;
  gobject_class->dispose = nbtk_widget_dispose;
  gobject_class->finalize = nbtk_widget_finalize;

  actor_class->allocate = nbtk_widget_allocate;
  actor_class->paint = nbtk_widget_paint;
  actor_class->parent_set = nbtk_widget_parent_set;

  klass->draw_background = nbtk_widget_real_draw_background;

  klass->style_changed = nbtk_widget_style_changed;
  klass->dnd_dropped = nbtk_widget_dnd_dropped;

  /**
   * NbtkWidget:pseudo-class:
   *
   * The pseudo-class of the actor. Typical values include "hover", "active",
   * "focus".
   */
  g_object_class_install_property (gobject_class,
                                   PROP_PSEUDO_CLASS,
                                   g_param_spec_string ("pseudo-class",
                                                        "Pseudo Class",
                                                        "Pseudo class for styling",
                                                        "",
                                                        NBTK_PARAM_READWRITE));
  /**
   * NbtkWidget:style-class:
   *
   * The style-class of the actor for use in styling.
   */
  g_object_class_install_property (gobject_class,
                                   PROP_STYLE_CLASS,
                                   g_param_spec_string ("style-class",
                                                        "Style Class",
                                                        "Style class for styling",
                                                        "",
                                                        NBTK_PARAM_READWRITE));

  g_object_class_override_property (gobject_class, PROP_STYLE, "style");

  /**
   * NbtkWidget:dnd-threshold:
   *
   * The threshold which needs to be exceeded before motion is treated as
   * drag. Value of 0 indicates that drag and drop is disabled.
   */
  g_object_class_install_property (gobject_class,
                                   PROP_DND_THRESHOLD,
                                   g_param_spec_uint ("dnd-threshold",
						      "DND threshold",
						      "DND threshold",
                                                       0, G_MAXUINT, 0,
						      NBTK_PARAM_READWRITE));


  /**
   * NbtkWidget:dnd-icon:
   *
   * An #ClutterActor icon to use during drag and drop. When this property is
   * not set, a clone of the dragged item is used.
   */
  g_object_class_install_property (gobject_class,
                                   PROP_DND_ICON,
                                   g_param_spec_object ("dnd-icon",
							"DND Icon",
							"Icon to use for DND",
							CLUTTER_TYPE_ACTOR,
							NBTK_PARAM_READWRITE));



  /**
   * NbtkWidget:stylable:
   *
   * Enable or disable styling of the widget
   */
  pspec = g_param_spec_boolean ("stylable",
                                "Stylable",
                                "Whether the table should be styled",
                                TRUE,
                                NBTK_PARAM_READWRITE);
  g_object_class_install_property (gobject_class,
                                   PROP_STYLABLE,
                                   pspec);

  /**
   * NbtkWidget::style-changed:
   * @actor: the actor that received the signal
   *
   * The ::style-changed signal will be emitted each time the style for the
   * object changes. This includes when any of the properties linked to the
   * style change, including #pseudo-class and #style-class
   */
  actor_signals[STYLE_CHANGED] =
    g_signal_new (I_("style-changed"),
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (NbtkWidgetClass, style_changed),
                  NULL, NULL,
                  _nbtk_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);

  /**
   * NbtkWidget::dnd-begin:
   * @actor: the #ClutterActor that received the signal
   * @dragged: #ClutterActor that is being dragged.
   * @icon: #ClutterActor representing the actor being dragged.
   * @x:     x coordinate of the event
   * @y:     y coordinate of the event
   *
   * The ::dnd-begin signal will be emitted each time a child of the container
   * enters into drag and drop state. When the drag and drop state ends,
   * the ::dnd-end signal is issued.
   */
  actor_signals[DND_BEGIN] =
    g_signal_new (I_("dnd-begin"),
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (NbtkWidgetClass, dnd_begin),
                  NULL, NULL,
                  _nbtk_marshal_VOID__OBJECT_OBJECT_INT_INT,
                  G_TYPE_NONE, 4,
		  CLUTTER_TYPE_ACTOR, CLUTTER_TYPE_ACTOR,
		  G_TYPE_INT, G_TYPE_INT);

  /**
   * NbtkWidget::dnd-motion:
   * @actor: #ClutterActor that received the signal
   * @dragged: #ClutterActor that is being dragged.
   * @icon: #ClutterActor representing the actor being dragged.
   * @x:     x coordinate of the event
   * @y:     y coordinate of the event
   *
   * The ::dnd-motion signal will be emitted each time a child of the container
   * changes is position due to dragging.
   */
  actor_signals[DND_MOTION] =
    g_signal_new (I_("dnd-motion"),
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (NbtkWidgetClass, dnd_motion),
                  NULL, NULL,
                  _nbtk_marshal_VOID__OBJECT_OBJECT_INT_INT,
                  G_TYPE_NONE, 4,
		  CLUTTER_TYPE_ACTOR, CLUTTER_TYPE_ACTOR,
		  G_TYPE_INT, G_TYPE_INT);

  /**
   * NbtkWidget::dnd-end:
   * @actor: #ClutterActor that received the signal
   * @dragged: #ClutterActor that is being dragged.
   * @icon: #ClutterActor representing the actor being dragged.
   * @x:     x coordinate of the event
   * @y:     y coordinate of the event
   *
   * The ::dnd-end signal will be emitted each time a child of the container
   * exists a drag and drop state. If the signal is issued due to successful
   * drop, the ::dnd-drop signal will be issued before the ::dnd-end signal.
   */
  actor_signals[DND_END] =
    g_signal_new (I_("dnd-end"),
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (NbtkWidgetClass, dnd_end),
                  NULL, NULL,
                  _nbtk_marshal_VOID__OBJECT_OBJECT_INT_INT,
                  G_TYPE_NONE, 4,
		  CLUTTER_TYPE_ACTOR, CLUTTER_TYPE_ACTOR,
		  G_TYPE_INT, G_TYPE_INT);

  /**
   * NbtkWidget::dnd-dropped:
   * @actor: #ClutterActor that received the signal
   * @dragged: #ClutterActor that is being dragged.
   * @icon: #ClutterActor representing the actor being dragged.
   * @x:     x coordinate of the event
   * @y:     y coordinate of the event
   *
   * The ::dnd-drop signal will be emitted before the ::dnd-end signal if
   * the drag resulted in a successful drop.
   */
  actor_signals[DND_DROPPED] =
    g_signal_new (I_("dnd-dropped"),
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (NbtkWidgetClass, dnd_dropped),
                  NULL, NULL,
                  _nbtk_marshal_VOID__OBJECT_OBJECT_INT_INT,
                  G_TYPE_NONE, 4,
		  CLUTTER_TYPE_ACTOR, CLUTTER_TYPE_ACTOR,
		  G_TYPE_INT, G_TYPE_INT);

  /**
   * NbtkWidget::dnd-enter:
   * @actor: #ClutterActor that received the signal
   * @dragged: #ClutterActor that is being dragged.
   * @icon: #ClutterActor representing the actor being dragged.
   * @x:     x coordinate of the event
   * @y:     y coordinate of the event
   *
   * The ::dnd-enter signal will be emitted each time the dragged child enters
   * the bounding box of a dnd-enabled NbtkWidget.
   */
  actor_signals[DND_ENTER] =
    g_signal_new (I_("dnd-enter"),
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (NbtkWidgetClass, dnd_enter),
                  NULL, NULL,
                  _nbtk_marshal_VOID__OBJECT_OBJECT_INT_INT,
                  G_TYPE_NONE, 4,
		  CLUTTER_TYPE_ACTOR, CLUTTER_TYPE_ACTOR,
		  G_TYPE_INT, G_TYPE_INT);

  /**
   * NbtkWidget::dnd-leave:
   * @actor: #ClutterActor that received the signal
   * @dragged: #ClutterActor that is being dragged.
   * @icon: #ClutterActor representing the actor being dragged.
   * @x:     x coordinate of the event
   * @y:     y coordinate of the event
   *
   * The ::dnd-leave signal will be emitted each time a dragged child of the
   * container leaves the bounding box of a dnd-enabled NbtkWidget.
   */
  actor_signals[DND_LEAVE] =
    g_signal_new (I_("dnd-leave"),
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (NbtkWidgetClass, dnd_leave),
                  NULL, NULL,
                  _nbtk_marshal_VOID__OBJECT_OBJECT_INT_INT,
                  G_TYPE_NONE, 4,
		  CLUTTER_TYPE_ACTOR, CLUTTER_TYPE_ACTOR,
		  G_TYPE_INT, G_TYPE_INT);
}

static NbtkStyle *
nbtk_widget_get_style (NbtkStylable *stylable)
{
  NbtkWidgetPrivate *priv = NBTK_WIDGET (stylable)->priv;

  return priv->style;
}

static void
nbtk_style_changed_cb (NbtkStyle    *style,
                       NbtkStylable *stylable)
{
  g_signal_emit (stylable, actor_signals[STYLE_CHANGED], 0, NULL);
}


static void
nbtk_widget_set_style (NbtkStylable *stylable,
                       NbtkStyle    *style)
{
  NbtkWidgetPrivate *priv = NBTK_WIDGET (stylable)->priv;

  if (priv->style)
    g_object_unref (priv->style);

  priv->style = g_object_ref_sink (style);

  g_signal_connect (priv->style,
                    "changed",
                    G_CALLBACK (nbtk_style_changed_cb),
                    stylable);
}

static NbtkStylable*
nbtk_widget_get_container (NbtkStylable *stylable)
{
  ClutterActor *parent;

  g_return_val_if_fail (NBTK_IS_WIDGET (stylable), NULL);

  parent = clutter_actor_get_parent (CLUTTER_ACTOR (stylable));

  if (NBTK_IS_STYLABLE (parent))
    return NBTK_STYLABLE (parent);
  else
    return NULL;
}

static NbtkStylable*
nbtk_widget_get_base_style (NbtkStylable *stylable)
{
  return NULL;
}

static const gchar*
nbtk_widget_get_style_id (NbtkStylable *stylable)
{
  g_return_val_if_fail (NBTK_IS_WIDGET (stylable), NULL);

  return clutter_actor_get_name (CLUTTER_ACTOR (stylable));
}

static const gchar*
nbtk_widget_get_style_type (NbtkStylable *stylable)
{
  return G_OBJECT_TYPE_NAME (stylable);
}

static const gchar*
nbtk_widget_get_style_class (NbtkStylable *stylable)
{
  g_return_val_if_fail (NBTK_IS_WIDGET (stylable), NULL);

  return NBTK_WIDGET (stylable)->priv->style_class;
}

static const gchar*
nbtk_widget_get_pseudo_class (NbtkStylable *stylable)
{
  g_return_val_if_fail (NBTK_IS_WIDGET (stylable), NULL);

  return NBTK_WIDGET (stylable)->priv->pseudo_class;
}

static gboolean
nbtk_widget_get_viewport (NbtkStylable *stylable,
                          gint *x,
                          gint *y,
                          gint *width,
                          gint *height)
{
  g_return_val_if_fail (NBTK_IS_WIDGET (stylable), FALSE);

  *x = 0;
  *y = 0;

  *width = clutter_actor_get_width (CLUTTER_ACTOR (stylable));
  *height = clutter_actor_get_height (CLUTTER_ACTOR (stylable));

  return TRUE;
}

/**
 * nbtk_widget_set_style_class_name:
 * @actor: a #NbtkWidget
 * @style_class: a new style class string
 *
 * Set the style class name
 */
void
nbtk_widget_set_style_class_name (NbtkWidget  *actor,
                                  const gchar *style_class)
{
  NbtkWidgetPrivate *priv = actor->priv;

  g_return_if_fail (NBTK_WIDGET (actor));

  priv = actor->priv;

  if (g_strcmp0 (style_class, priv->style_class))
    {
      g_free (priv->style_class);
      priv->style_class = g_strdup (style_class);

      g_signal_emit (actor, actor_signals[STYLE_CHANGED], 0);

      g_object_notify (G_OBJECT (actor), "style-class");
    }
}


/**
 * nbtk_widget_get_style_class_name:
 * @actor: a #NbtkWidget
 *
 * Get the current style class name
 *
 * Returns: the class name string. The string is owned by the #NbtkWidget and
 * should not be modified or freed.
 */
const gchar*
nbtk_widget_get_style_class_name (NbtkWidget *actor)
{
  g_return_val_if_fail (NBTK_WIDGET (actor), NULL);

  return actor->priv->style_class;
}

/**
 * nbtk_widget_get_style_pseudo_class:
 * @actor: a #NbtkWidget
 *
 * Get the current style pseudo class
 *
 * Returns: the pseudo class string. The string is owned by the #NbtkWidget and
 * should not be modified or freed.
 */
const gchar*
nbtk_widget_get_style_pseudo_class (NbtkWidget *actor)
{
  g_return_val_if_fail (NBTK_WIDGET (actor), NULL);

  return actor->priv->pseudo_class;
}

/**
 * nbtk_widget_set_style_pseudo_class:
 * @actor: a #NbtkWidget
 * @pseudo_class: a new pseudo class string
 *
 * Set the style pseudo class
 */
void
nbtk_widget_set_style_pseudo_class (NbtkWidget  *actor,
                                    const gchar *pseudo_class)
{
  NbtkWidgetPrivate *priv;

  g_return_if_fail (NBTK_WIDGET (actor));

  priv = actor->priv;

  if (g_strcmp0 (pseudo_class, priv->pseudo_class))
    {
      g_free (priv->pseudo_class);
      priv->pseudo_class = g_strdup (pseudo_class);

      g_signal_emit (actor, actor_signals[STYLE_CHANGED], 0);

      g_object_notify (G_OBJECT (actor), "pseudo-class");
    }
}


static void
nbtk_stylable_iface_init (NbtkStylableIface *iface)
{
  static gboolean is_initialized = FALSE;

  if (!is_initialized)
    {
      GParamSpec *pspec;
      ClutterColor color = { 0x00, 0x00, 0x00, 0xff };
      ClutterColor bg_color = { 0xff, 0xff, 0xff, 0x00 };

      is_initialized = TRUE;

      pspec = clutter_param_spec_color ("background-color",
                                  "Background Color",
                                  "The background color of an actor",
                                  &bg_color,
                                  G_PARAM_READWRITE);
      nbtk_stylable_iface_install_property (iface, NBTK_TYPE_WIDGET, pspec);

      pspec = clutter_param_spec_color ("color",
                                  "Text Color",
                                  "The color of the text of an actor",
                                  &color,
                                  G_PARAM_READWRITE);
      nbtk_stylable_iface_install_property (iface, NBTK_TYPE_WIDGET, pspec);

      pspec = g_param_spec_string ("background-image",
                                   "Background Image",
                                   "Background image filename",
                                   NULL,
                                   G_PARAM_READWRITE);
      nbtk_stylable_iface_install_property (iface, NBTK_TYPE_WIDGET, pspec);

      pspec = g_param_spec_string ("font-family",
                                   "Font Family",
                                   "Name of the font to use",
                                   "Sans",
                                   G_PARAM_READWRITE);
      nbtk_stylable_iface_install_property (iface, NBTK_TYPE_WIDGET, pspec);

      pspec = g_param_spec_int ("font-size",
                                "Font Size",
                                "Size of the font to use in pixels",
                                0, G_MAXINT, 12,
                                G_PARAM_READWRITE);
      nbtk_stylable_iface_install_property (iface, NBTK_TYPE_WIDGET, pspec);

      pspec = g_param_spec_boxed ("border-image",
                                  "Border image",
                                  "9-slice image to use for drawing borders and background",
                                  NBTK_TYPE_BORDER_IMAGE,
                                  G_PARAM_READWRITE);
      nbtk_stylable_iface_install_property (iface, NBTK_TYPE_WIDGET, pspec);

      pspec = g_param_spec_boxed ("padding",
                                  "Padding",
                                  "Padding between the widget's borders "
                                  "and its content",
                                  NBTK_TYPE_PADDING,
                                  G_PARAM_READWRITE);
      nbtk_stylable_iface_install_property (iface, NBTK_TYPE_WIDGET, pspec);

      iface->get_style = nbtk_widget_get_style;
      iface->set_style = nbtk_widget_set_style;
      iface->get_base_style = nbtk_widget_get_base_style;
      iface->get_container = nbtk_widget_get_container;
      iface->get_style_id = nbtk_widget_get_style_id;
      iface->get_style_type = nbtk_widget_get_style_type;
      iface->get_style_class = nbtk_widget_get_style_class;
      iface->get_pseudo_class = nbtk_widget_get_pseudo_class;
      /* iface->get_attribute = nbtk_widget_get_attribute; */
      iface->get_viewport = nbtk_widget_get_viewport;
    }
}


static void
nbtk_widget_name_notify (NbtkWidget *widget,
                         GParamSpec *pspec,
                         gpointer data)
{
  g_signal_emit (widget, actor_signals[STYLE_CHANGED], 0);
}

static void
nbtk_widget_init (NbtkWidget *actor)
{
  NbtkWidgetPrivate *priv;

  actor->priv = priv = NBTK_WIDGET_GET_PRIVATE (actor);
  priv->is_stylable = TRUE;

  clutter_actor_set_reactive (CLUTTER_ACTOR (actor), TRUE);

  /* connect style changed */
  g_signal_connect (actor, "notify::name", G_CALLBACK (nbtk_widget_name_notify), NULL);

  /* set the default style */
  nbtk_widget_set_style (NBTK_STYLABLE (actor), nbtk_style_get_default ());

}

/**
 * nbtk_widget_get_dnd_threshold:
 * @actor: a #NbtkWidget
 *
 * Retrieves the drag and drop threshold.
 *
 * Returns: the current threshold.
 */
guint
nbtk_widget_get_dnd_threshold (NbtkWidget *actor)
{
  g_return_val_if_fail (NBTK_IS_WIDGET (actor), 0);

  return actor->priv->dnd_threshold;
}

/**
 * nbtk_widget_set_dnd_threshold:
 * @actor: a #NbtkWidget
 * @threshold: the threshold.
 *
 * Sets the drag and drop threshold.
 */
void
nbtk_widget_set_dnd_threshold (NbtkWidget *actor, guint threshold)
{
  NbtkWidgetPrivate *priv;

  g_return_if_fail (NBTK_IS_WIDGET (actor));

  priv = actor->priv;

  if (priv->dnd_threshold != threshold)
    {
      priv->dnd_threshold = threshold;

      g_object_notify (G_OBJECT (actor), "dnd-threshold");
    }
}

static void
nbtk_widget_dnd_last_dest_weak_cb (gpointer data, GObject *last_dest)
{
  NbtkWidgetPrivate *priv = NBTK_WIDGET (data)->priv;

  priv->dnd_last_dest = NULL;
  priv->dnd_last_dest_data = NULL;
}

static gboolean
nbtk_widget_dnd_enter_event_cb (ClutterActor *actor,
				ClutterCrossingEvent *event,
				gpointer data)
{
  ClutterActor *dest = event->source;
  NbtkWidgetPrivate *priv = NBTK_WIDGET (data)->priv;

  while (dest && (!NBTK_IS_WIDGET (dest) ||
		  !NBTK_WIDGET (dest)->priv->dnd_threshold))
    dest = clutter_actor_get_parent (dest);

  if (dest && NBTK_IS_WIDGET (dest) && NBTK_WIDGET (dest)->priv->dnd_threshold)
    {
      if (dest != priv->dnd_last_dest)
	{
	  if (priv->dnd_last_dest)
	    {
	      g_object_ref (priv->dnd_last_dest);

	      g_signal_emit (priv->dnd_last_dest, actor_signals[DND_LEAVE], 0,
			     priv->dnd_dragged,
			     priv->dnd_clone, event->x, event->y);

	      g_object_unref (priv->dnd_last_dest);
	    }

	  g_object_ref (dest);

	  g_signal_emit (dest, actor_signals[DND_ENTER], 0,
			 priv->dnd_dragged,
			 priv->dnd_clone, event->x, event->y);

	  g_object_unref (dest);

          if (priv->dnd_last_dest)
            g_object_weak_unref (G_OBJECT (priv->dnd_last_dest),
                                 nbtk_widget_dnd_last_dest_weak_cb,
                                 priv->dnd_last_dest_data);

	  priv->dnd_last_dest = dest;
          priv->dnd_last_dest_data = data;

          g_object_weak_ref (G_OBJECT (dest),
                             nbtk_widget_dnd_last_dest_weak_cb, data);
	}
    }
  else if (priv->dnd_last_dest)
    {
      g_object_ref (priv->dnd_last_dest);

      g_signal_emit (priv->dnd_last_dest, actor_signals[DND_LEAVE], 0,
		     priv->dnd_dragged,
		     priv->dnd_clone, event->x, event->y);

      g_object_weak_unref (G_OBJECT (priv->dnd_last_dest),
                           nbtk_widget_dnd_last_dest_weak_cb,
                           priv->dnd_last_dest_data);

      g_object_unref (priv->dnd_last_dest);

      priv->dnd_last_dest = NULL;
      priv->dnd_last_dest_data = NULL;
    }

  return TRUE;
}

static gboolean
nbtk_widget_child_dnd_motion_cb (ClutterActor *child,
				 ClutterEvent *event,
				 gpointer      data);

static gboolean
nbtk_widget_child_dnd_release_cb (ClutterActor *child,
				  ClutterEvent *event,
				  gpointer      data)
{
  NbtkWidget *widget = data;
  NbtkWidgetPrivate *priv;
  gboolean retval = FALSE;

  if (event->type != CLUTTER_BUTTON_RELEASE || event->button.button != 1)
    return FALSE;

  priv = NBTK_WIDGET (widget)->priv;

  if (priv->dnd_motion)
    {
      ClutterActor *dest;
      ClutterStage *stage;
      ClutterActor *clone;

      gint x = event->motion.x;
      gint y = event->motion.y;

      clone = priv->dnd_clone;

      /*
       * Hide the clone, so it does not interfer with picking.
       */
      clutter_actor_hide (clone);

      stage = CLUTTER_STAGE (clutter_actor_get_stage (child));

      dest = clutter_stage_get_actor_at_pos (stage, x, y);

      g_object_ref (child);
      /* We do not need reference on the clone, as we already have one.*/

      if (dest)
	{
	  /*
	   * If the target is not NbtkWidget, or the widget does not have dnd
	   * enabled, we try to propagate the signal down the ancestry chain.
	   */
	  if (!NBTK_IS_WIDGET (dest) ||
	      !NBTK_WIDGET (dest)->priv->dnd_threshold)
	    {
	      dest = clutter_actor_get_parent (dest);

	      while (dest && (!NBTK_IS_WIDGET (dest) ||
			      !NBTK_WIDGET (dest)->priv->dnd_threshold))
		dest = clutter_actor_get_parent (dest);
	    }

	  if (dest)
	    {
	      g_object_ref (dest);

	      g_signal_emit (dest, actor_signals[DND_DROPPED], 0, child, clone,
			     x, y);

	      g_object_unref (dest);
	    }
	}

      if (priv->dnd_enter_cb_id)
	{
	  g_signal_handler_disconnect (priv->dnd_dragged,
				       priv->dnd_enter_cb_id);
	  priv->dnd_enter_cb_id = 0;
	}

      if (priv->dnd_last_dest)
        {
          g_object_ref (priv->dnd_last_dest);

          g_signal_emit (priv->dnd_last_dest, actor_signals[DND_LEAVE], 0,
                         priv->dnd_dragged,
                         priv->dnd_clone, event->button.x, event->button.y);

          g_object_weak_unref (G_OBJECT (priv->dnd_last_dest),
                               nbtk_widget_dnd_last_dest_weak_cb,
                               priv->dnd_last_dest_data);

          g_object_unref (priv->dnd_last_dest);

          priv->dnd_last_dest = NULL;
          priv->dnd_last_dest_data = NULL;
        }

      g_signal_emit (widget, actor_signals[DND_END], 0,
		     child, clone, x, y);


      g_object_unref (child);
      g_object_unref (clone); /* The extra ref we got when we created it. */

      if (priv->dnd_clone)
	{
          ClutterActor *parent = clutter_actor_get_parent (clone);

          priv->dnd_clone = NULL;

          if (parent)
            {
              if (CLUTTER_IS_CONTAINER (parent))
                clutter_container_remove_actor (CLUTTER_CONTAINER (parent),
                                                clone);
              else
                clutter_actor_unparent (clone);
            }
        }

      retval = TRUE;
    }

  if (priv->dnd_grab)
    clutter_ungrab_pointer ();

  priv->dnd_motion = FALSE;
  priv->dnd_grab = FALSE;

  g_signal_handlers_disconnect_by_func (child,
                                        nbtk_widget_child_dnd_motion_cb,
                                        data);

  if (priv->dnd_dragged)
    {
      ClutterActor *dragged = priv->dnd_dragged;

      priv->dnd_dragged = NULL;
      g_object_unref (dragged);
    }

  g_object_unref (widget);

  return retval;
}

static gboolean
nbtk_widget_child_dnd_motion_cb (ClutterActor *child,
				 ClutterEvent *event,
				 gpointer      data)
{
  NbtkWidget *widget = data;
  NbtkWidgetPrivate *priv = NBTK_WIDGET (widget)->priv;
  gint x = event->motion.x;
  gint y = event->motion.y;
  gint dx = x - priv->dnd_x;
  gint dy = y - priv->dnd_y;
  guint threshold = priv->dnd_threshold;
  ClutterStage *stage;

  if (dx < threshold && dy < threshold)
    return FALSE;

  stage = CLUTTER_STAGE (clutter_actor_get_stage (CLUTTER_ACTOR (widget)));

  if (!priv->dnd_motion)
    {
      ClutterActor *clone;
      gdouble scale_x, scale_y;
      guint child_w, child_h;

      if (priv->dnd_last_dest)
        {
          g_warning ("There should be no last destination set at this point\n");

          g_object_weak_unref (G_OBJECT (priv->dnd_last_dest),
                               nbtk_widget_dnd_last_dest_weak_cb,
                               priv->dnd_last_dest_data);
        }

      priv->dnd_last_dest = CLUTTER_ACTOR (widget);
      priv->dnd_last_dest_data = data;

      g_object_weak_ref (G_OBJECT (priv->dnd_last_dest),
                         nbtk_widget_dnd_last_dest_weak_cb, data);

      if (priv->dnd_icon)
	{
	  clone = priv->dnd_icon;
	}
      else
	{
	  clone = clutter_clone_new (child);

	  clutter_actor_get_scale (child, &scale_x, &scale_y);
	  clutter_actor_set_scale (clone, scale_x, scale_y);

	  clutter_actor_get_size  (child, &child_w, &child_h);
	  clutter_actor_set_size  (clone, child_w, child_h);
	}

      g_signal_connect (child, "button-release-event",
			G_CALLBACK (nbtk_widget_child_dnd_release_cb), widget);

      priv->dnd_clone = clone;

      clutter_actor_set_position (clone, x, y);

      clutter_container_add_actor (CLUTTER_CONTAINER (stage), clone);
      g_object_ref (clone);

      clutter_actor_show (clone);

      g_object_ref (widget);

      g_signal_emit (widget, actor_signals[DND_BEGIN], 0, child, clone, x, y);

      g_object_unref (widget);

      priv->dnd_motion = TRUE;
    }
  else
    {
      clutter_actor_move_by (priv->dnd_clone, dx, dy);
      clutter_actor_queue_redraw (priv->dnd_clone);
    }

  g_object_ref (widget);

  g_signal_emit (widget, actor_signals[DND_MOTION], 0,
		 child, priv->dnd_clone, x, y);

  g_object_unref (widget);

  priv->dnd_x = x;
  priv->dnd_y = y;

  return TRUE;
}

static gboolean
nbtk_widget_child_dnd_press_cb (ClutterActor *child,
				ClutterEvent *event,
				gpointer      data)
{
  NbtkWidget *widget = data;
  NbtkWidgetPrivate *priv = NBTK_WIDGET (widget)->priv;
  guint threshold = priv->dnd_threshold;

  if (!threshold || event->button.button != 1 || event->button.click_count > 1)
    return FALSE;

  g_object_ref (data);

  priv->dnd_x = event->button.x;
  priv->dnd_y = event->button.y;

  priv->dnd_motion = FALSE;
  priv->dnd_grab = TRUE;

  priv->dnd_dragged = g_object_ref (child);

  clutter_grab_pointer (child);

  g_signal_connect (child, "motion-event",
                    G_CALLBACK (nbtk_widget_child_dnd_motion_cb), data);

  priv->dnd_enter_cb_id =
    g_signal_connect (child, "enter-event",
		      G_CALLBACK (nbtk_widget_dnd_enter_event_cb), data);

  return TRUE;
}

ClutterActor *
_nbtk_widget_get_dnd_clone (NbtkWidget *widget)
{
  NbtkWidgetPrivate *priv = NBTK_WIDGET (widget)->priv;

  return priv->dnd_clone;
}


/**
 * nbtk_widget_setup_child_dnd:
 * @actor: a #NbtkWidget
 * @child: a #ClutterActor
 *
 * Sets up child for drag and drop; this function is intended
 * for use by NbtkWidget subclasses that wish to support DND.
 */
void
nbtk_widget_setup_child_dnd (NbtkWidget *actor, ClutterActor *child)
{
  NbtkWidgetChild *meta;

  meta = NBTK_WIDGET_CHILD (
	clutter_container_get_child_meta (CLUTTER_CONTAINER (actor), child));

  if (!meta || meta->dnd_disabled)
    return;

  g_signal_connect (child, "button-press-event",
		    G_CALLBACK (nbtk_widget_child_dnd_press_cb), actor);
}

/**
 * nbtk_widget_undo_child_dnd:
 * @actor: a #NbtkWidget
 * @child: a #ClutterActor
 *
 * Removes the DND machinery from the child; this function is intended
 * for use by NbtkWidget subclasses that wish to support DND.
 */
void
nbtk_widget_undo_child_dnd (NbtkWidget *actor, ClutterActor *child)
{
  g_signal_handlers_disconnect_by_func (child,
                                        nbtk_widget_child_dnd_press_cb,
                                        actor);
  g_signal_handlers_disconnect_by_func (child,
                                        nbtk_widget_child_dnd_release_cb,
                                        actor);
}

static NbtkBorderImage *
nbtk_border_image_copy (const NbtkBorderImage *border_image)
{
  NbtkBorderImage *copy;

  g_return_val_if_fail (border_image != NULL, NULL);

  copy = g_slice_new (NbtkBorderImage);
  *copy = *border_image;

  return copy;
}

static void
nbtk_border_image_free (NbtkBorderImage *border_image)
{
  if (G_LIKELY (border_image))
    g_slice_free (NbtkBorderImage, border_image);
}

GType
nbtk_border_image_get_type (void)
{
  static GType our_type = 0;

  if (G_UNLIKELY (our_type == 0))
    our_type =
      g_boxed_type_register_static (I_("NbtkBorderImage"),
                                    (GBoxedCopyFunc) nbtk_border_image_copy,
                                    (GBoxedFreeFunc) nbtk_border_image_free);

  return our_type;
}

G_GNUC_DEPRECATED void
nbtk_widget_get_border (NbtkWidget *actor,
                        NbtkPadding *border)
{
  g_warning ("%s is deprecated and may be removed in the future.",
             __FUNCTION__);
}

/**
 * nbtk_widget_get_border_image:
 * @actor: A #NbtkWidget
 *
 * Get the texture used as the border image. This is set using the
 * "border-image" CSS property. This function should normally only be used
 * by subclasses.
 *
 * Returns: #ClutterActor
 */
ClutterActor *
nbtk_widget_get_border_image (NbtkWidget *actor)
{
  NbtkWidgetPrivate *priv = NBTK_WIDGET (actor)->priv;
  return priv->border_image;
}

/**
 * nbtk_widget_get_background_image:
 * @actor: A #NbtkWidget
 *
 * Get the texture used as the background image. This is set using the
 * "background-image" CSS property. This function should normally only be used
 * by subclasses.
 *
 * Returns: a #ClutterActor
 */
ClutterActor *
nbtk_widget_get_background_image (NbtkWidget *actor)
{
  NbtkWidgetPrivate *priv = NBTK_WIDGET (actor)->priv;
  return priv->background_image;
}

/**
 * nbtk_widget_get_padding:
 * @widget: A #NbtkWidget
 * @padding: A pointer to an #NbtkPadding to fill
 *
 * Gets the padding of the widget, set using the "padding" CSS property. This
 * function should normally only be used by subclasses.
 *
 */
void
nbtk_widget_get_padding (NbtkWidget *widget,
                         NbtkPadding *padding)
{
  g_return_if_fail (NBTK_IS_WIDGET (widget));
  g_return_if_fail (padding != NULL);

  *padding = widget->priv->padding;
}
