/*
 * mx-slider.c: Slider widget
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
 *
 * Written by: Damien Lespiau <damien.lespiau@intel.com>
 *
 */

/**
 * SECTION:mx-slider
 * @short_description: A widget to visualize and control a range
 *
 * FIXME
 *
 */

#include "mx-slider.h"
#include "mx-stylable.h"
#include "mx-progress-bar-fill.h"
#include "mx-button.h"
#include "mx-frame.h"

static void mx_stylable_iface_init (MxStylableIface *iface);

G_DEFINE_TYPE_WITH_CODE (MxSlider, mx_slider, MX_TYPE_WIDGET,
                         G_IMPLEMENT_INTERFACE (MX_TYPE_STYLABLE,
                                                mx_stylable_iface_init))


#define SLIDER_PRIVATE(o)                         \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o),              \
                                MX_TYPE_SLIDER,   \
                                MxSliderPrivate))

#define DEFAULT_HANDLE_WIDTH        25
#define DEFAULT_HANDLE_HEIGHT       16

struct _MxSliderPrivate
{
  ClutterActor *trough_bg;
  ClutterActor *fill;
  ClutterActor *trough;
  ClutterActor *handle;

  gulong        capture_handler;
  gfloat        x_origin;

  /* the middle of the handle can wander on the axis between start and end */
  gfloat        handle_middle_start;
  gfloat        handle_middle_end;

  /* keep those around for ::alocate_fill() */
  gfloat        trough_box_y1;
  gfloat        trough_box_y2;
  gint          trough_height;
  guint         handle_width;
  guint         handle_height;

  gdouble       progress;
};

enum
{
  PROP_0,

  PROP_PROGRESS
};

static void
drag_handle (MxSlider *bar,
             gfloat    x,
             gfloat    y)
{
  MxSliderPrivate *priv = bar->priv;
  gdouble progress;
  gfloat ux, pos, handle_width_2, fill_size, offset;

  if (!clutter_actor_transform_stage_point (CLUTTER_ACTOR (bar),
                                            x, y,
                                            &ux, NULL))
    {
      return;
    }

  fill_size = priv->handle_middle_end - priv->handle_middle_start;

  /* offset is the difference between the middle of the handle and where one
   * clicked on it */
  handle_width_2 = clutter_actor_get_width (priv->handle) / 2;
  offset = handle_width_2 - priv->x_origin;

  pos = ux - priv->handle_middle_start + offset;
  pos = CLAMP (pos, 0, fill_size);

  progress = pos / fill_size;
  mx_slider_set_progress (bar, progress);
}

static gboolean
on_handle_capture_event (ClutterActor *trough,
                         ClutterEvent *event,
                         MxSlider     *bar)
{
  MxSliderPrivate *priv = bar->priv;

  if (clutter_event_type (event) == CLUTTER_MOTION)
    {
      drag_handle (bar,
                   ((ClutterMotionEvent*)event)->x,
                   ((ClutterMotionEvent*)event)->y);
    }
  else if (clutter_event_type (event) == CLUTTER_BUTTON_RELEASE
           && ((ClutterButtonEvent*)event)->button == 1)
    {
      ClutterActor *stage, *target;

      stage = clutter_actor_get_stage(priv->trough);

      if (priv->capture_handler)
        {
          g_signal_handler_disconnect (stage, priv->capture_handler);
          priv->capture_handler = 0;
        }

      clutter_set_motion_events_enabled (TRUE);

      /* check if the mouse pointer has left the handle during the drag and
       * remove the hover state if it has */
      target =
        clutter_stage_get_actor_at_pos ((ClutterStage*) stage,
                                        CLUTTER_PICK_REACTIVE,
                                        ((ClutterButtonEvent*) event)->x,
                                        ((ClutterButtonEvent*) event)->y);
      if (target != priv->handle)
        {
          mx_stylable_set_style_pseudo_class (MX_STYLABLE (priv->handle),
                                            NULL);
        }
    }

  return TRUE;
}

static void
move_handle (MxSlider *bar,
             gfloat    x,
             gfloat    y)
{
  MxSliderPrivate *priv = bar->priv;
  gdouble progress;
  gfloat ux, pos, fill_size;

  if (!clutter_actor_transform_stage_point (CLUTTER_ACTOR (bar),
                                            x, y,
                                            &ux, NULL))
    {
      return;
    }

  priv->x_origin = priv->handle_width / 2 + clutter_actor_get_x (priv->trough);

  fill_size = priv->handle_middle_end - priv->handle_middle_start;
  pos = ux - priv->handle_middle_start;
  pos = CLAMP (pos, 0, fill_size);

  progress = pos / fill_size;
  mx_slider_set_progress (bar, progress);
}

static gboolean
on_trough_bg_button_press_event (ClutterActor       *actor,
                                 ClutterButtonEvent *event,
                                 MxSlider           *self)
{
  MxSliderPrivate *priv = self->priv;

  if (event->button != 1)
    return FALSE;

  move_handle (self, event->x, event->y);

  /* Turn off picking for motion events */
  clutter_set_motion_events_enabled (FALSE);

  priv->capture_handler =
    g_signal_connect_after (clutter_actor_get_stage (priv->handle),
                            "captured-event",
                            G_CALLBACK (on_handle_capture_event),
                            self);

  return TRUE;
}

static gboolean
on_trough_bg_button_release_event (ClutterActor       *actor,
                                   ClutterButtonEvent *event,
                                   MxSlider           *self)
{
  if (event->button != 1)
    return FALSE;

  return TRUE;
}

static gboolean
on_trough_bg_leave_event (ClutterActor *actor,
                          ClutterEvent *event,
                          MxSlider     *self)
{
  return TRUE;
}

static gboolean
on_handle_button_press_event (ClutterActor       *actor,
                              ClutterButtonEvent *event,
                              MxSlider           *bar)
{
  MxSliderPrivate *priv = bar->priv;

  if (event->button != 1)
    return FALSE;

  if (!clutter_actor_transform_stage_point (priv->handle,
                                            event->x,
                                            event->y,
                                            &priv->x_origin,
                                            NULL))
    return FALSE;

  /* Account for the scrollbar-trough-handle nesting. */
  priv->x_origin += clutter_actor_get_x (priv->trough);

  /* Turn off picking for motion events */
  clutter_set_motion_events_enabled (FALSE);

  priv->capture_handler =
    g_signal_connect_after (clutter_actor_get_stage (priv->trough),
                            "captured-event",
                            G_CALLBACK (on_handle_capture_event),
                            bar);

  return TRUE;
}

/*
 * MxStylable
 */

static void
mx_stylable_iface_init (MxStylableIface *iface)
{
  static gboolean is_initialized = FALSE;

  if (!is_initialized)
    {
      GParamSpec *pspec;

      is_initialized = TRUE;

      /* if specified, this will be the allocated height of the trough.
       * By default, the height of the trough is the same as its parent. */
      pspec = g_param_spec_int ("x-mx-trough-height",
                                "Height of the trough",
                                "Height of the trough, in px",
                                -1, G_MAXINT, -1,
                                G_PARAM_READWRITE);
      mx_stylable_iface_install_property (iface,
                                          MX_TYPE_SLIDER, pspec);

      /* FIXME: have a trough-width property too? */

      pspec = g_param_spec_uint ("x-mx-handle-width",
                                 "Handle width",
                                 "Width of the handle, in px",
                                 0, G_MAXUINT, DEFAULT_HANDLE_WIDTH,
                                 G_PARAM_READWRITE);
      mx_stylable_iface_install_property (iface,
                                          MX_TYPE_SLIDER, pspec);

      pspec = g_param_spec_uint ("x-mx-handle-height",
                                 "Handle height",
                                 "Height of the handle, in px",
                                 0, G_MAXUINT, DEFAULT_HANDLE_HEIGHT,
                                 G_PARAM_READWRITE);
      mx_stylable_iface_install_property (iface,
                                          MX_TYPE_SLIDER, pspec);
    }
}

/*
 * ClutterActor overloading
 */

static void
mx_slider_paint (ClutterActor *actor)
{
  MxSlider *self = MX_SLIDER (actor);
  MxSliderPrivate *priv = self->priv;

  CLUTTER_ACTOR_CLASS (mx_slider_parent_class)->paint (actor);

  clutter_actor_paint (priv->trough_bg);

  if (priv->progress)
    clutter_actor_paint (priv->fill);

  clutter_actor_paint (priv->trough);

  clutter_actor_paint (priv->handle);
}

static void
mx_slider_pick (ClutterActor       *actor,
                const ClutterColor *pick_color)
{
  MxSlider *self = MX_SLIDER (actor);
  MxSliderPrivate *priv = self->priv;

  /* Chaining up won't draw the media bar outline as it's not set reactive
   * by default */
  CLUTTER_ACTOR_CLASS (mx_slider_parent_class)->pick (actor, pick_color);

  clutter_actor_paint (priv->trough_bg);
  clutter_actor_paint (priv->handle);
}

static void
mx_slider_get_preferred_width (ClutterActor *actor,
                               gfloat        for_height,
                               gfloat       *min_width_p,
                               gfloat       *nat_width_p)
{
  MxPadding padding;
  MxSliderPrivate *priv = MX_SLIDER (actor)->priv;

  mx_widget_get_padding (MX_WIDGET (actor), &padding);

  /* Set the size of the handle + padding */
  if (min_width_p)
    *min_width_p = priv->handle_width + padding.left + padding.right;
  if (nat_width_p)
    *nat_width_p = priv->handle_width + padding.left + padding.right;
}

static void
mx_slider_get_preferred_height (ClutterActor *actor,
                                gfloat        for_width,
                                gfloat       *min_height_p,
                                gfloat       *nat_height_p)
{
  MxPadding padding;
  MxSliderPrivate *priv = MX_SLIDER (actor)->priv;

  mx_widget_get_padding (MX_WIDGET (actor), &padding);

  /* Add on the size of the trough/handle + padding */
  if (min_height_p)
    *min_height_p = MAX (priv->handle_height, priv->trough_height) +
                    padding.top + padding.bottom;
  if (nat_height_p)
    *nat_height_p = MAX (priv->handle_height, priv->trough_height) +
                    padding.top + padding.bottom;
}

static void
mx_slider_allocate_fill_handle (MxSlider               *self,
                                const ClutterActorBox  *box,
                                ClutterAllocationFlags  flags)
{
  MxSliderPrivate *priv = self->priv;
  MxPadding        padding;
  ClutterActorBox  bar_box;
  ClutterActorBox  fill_box;
  ClutterActorBox  handle_box;
  guint            handle_width_2;

  if (box == NULL)
    {
      clutter_actor_get_allocation_box (CLUTTER_ACTOR (self), &bar_box);
      box = &bar_box;
    }

  mx_widget_get_padding (MX_WIDGET (self), &padding);

  handle_width_2 = priv->handle_width >> 1;

  /* fill */
  fill_box.x1 = padding.left;
  fill_box.y1 = priv->trough_box_y1;
  fill_box.x2 = ((box->x2 - box->x1 - padding.left - padding.right -
                  priv->handle_width) * priv->progress) + padding.left +
                  handle_width_2;
  fill_box.x2 = CLAMP (fill_box.x2,
                       priv->handle_middle_start,
                       priv->handle_middle_end);
  fill_box.y2 = priv->trough_box_y2;

  clutter_actor_allocate (priv->fill, &fill_box, flags);

  /* handle */
  handle_box.x1 = fill_box.x2 - handle_width_2;
  handle_box.x2 = handle_box.x1 + priv->handle_width;

  /* If the handle height is unset, or the handle height is greater than the
   * trough height, we want the handle to occupy all available space.
   * Otherwise we want it to be centred in the trough.
   */
  if ((priv->handle_height == 0) || (priv->handle_height > priv->trough_height))
    {
      handle_box.y1 = padding.top;
      handle_box.y2 = (box->y2 - box->y1) - padding.bottom;
    }
  else
    {
      handle_box.y1 = (box->y2 - box->y1 - priv->handle_height) / 2.f;
      handle_box.y2 = handle_box.y1 + priv->handle_height;
    }

  /* snap to pixel */
  handle_box.x1 = (int) handle_box.x1;
  handle_box.y1 = (int) handle_box.y1;
  handle_box.x2 = (int) handle_box.x2;
  handle_box.y2 = (int) handle_box.y2;

  clutter_actor_allocate (priv->handle, &handle_box, flags);
}

static void
mx_slider_allocate (ClutterActor           *actor,
                    const ClutterActorBox  *box,
                    ClutterAllocationFlags  flags)
{
  MxSlider          *self = MX_SLIDER (actor);
  MxSliderPrivate   *priv = self->priv;
  MxPadding          padding;
  ClutterActorClass *actor_class;
  ClutterActorBox    bar_box;
  ClutterActorBox    trough_box;
  guint              handle_width_2;

  actor_class = CLUTTER_ACTOR_CLASS (mx_slider_parent_class);
  actor_class->allocate (actor, box, flags);

  if (box == NULL)
    {
      clutter_actor_get_allocation_box (CLUTTER_ACTOR (self), &bar_box);
      box = &bar_box;
    }

  handle_width_2 = priv->handle_width >> 1;

  mx_widget_get_padding (MX_WIDGET (self), &padding);

  /* save the min/max position of the middle of the handle */
  priv->handle_middle_start = padding.left + handle_width_2 + 1;
  priv->handle_middle_end   = box->x2 - box->x1 - padding.right -
    handle_width_2 - 1;

  if (priv->trough_height < 0)
    {
      /* trough-height has not been specified, take the whole height */
      trough_box.x1 = padding.left;
      trough_box.y1 = padding.top;
      trough_box.x2 = (box->x2 - box->x1) - padding.right;
      trough_box.y2 = (box->y2 - box->y1) - padding.bottom;
    }
  else
    {
      trough_box.x1 = padding.left;
      trough_box.y1 = (box->y2 - box->y1 - padding.bottom - padding.top -
                       priv->trough_height) / 2;
      trough_box.x2 = (box->x2 - box->x1) - padding.right;
      trough_box.y2 = trough_box.y1 + priv->trough_height;
    }

  clutter_actor_allocate (priv->trough_bg, &trough_box, flags);

  /* save trough_box.y1 and trough_box.y2 so we don't have the duplicate
   * the logic above in ::allocate_fill() */
  priv->trough_box_y1 = trough_box.y1;
  priv->trough_box_y2 = trough_box.y2;

  mx_slider_allocate_fill_handle (self, box, flags);

  clutter_actor_allocate (priv->trough, &trough_box, flags);
}

static void
mx_slider_map (ClutterActor *actor)
{
  MxSlider *self = MX_SLIDER (actor);
  MxSliderPrivate *priv = self->priv;

  CLUTTER_ACTOR_CLASS (mx_slider_parent_class)->map (actor);
  clutter_actor_map (priv->trough_bg);
  clutter_actor_map (priv->fill);
  clutter_actor_map (priv->trough);
  clutter_actor_map (priv->handle);
}

static void
mx_slider_unmap (ClutterActor *actor)
{
  MxSlider *self = MX_SLIDER (actor);
  MxSliderPrivate *priv = self->priv;

  CLUTTER_ACTOR_CLASS (mx_slider_parent_class)->unmap (actor);
  clutter_actor_unmap (priv->trough_bg);
  clutter_actor_unmap (priv->fill);
  clutter_actor_unmap (priv->trough);
  clutter_actor_unmap (priv->handle);
}

/*
 * GObject overloading
 */

static void
mx_slider_get_property (GObject    *object,
                        guint       property_id,
                        GValue     *value,
                        GParamSpec *pspec)
{
  MxSlider *self = MX_SLIDER (object);

  switch (property_id)
    {
    case PROP_PROGRESS:
      g_value_set_double (value, mx_slider_get_progress (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
mx_slider_set_property (GObject      *object,
                        guint         property_id,
                        const GValue *value,
                        GParamSpec   *pspec)
{
  MxSlider *self = MX_SLIDER (object);

  switch (property_id)
    {
    case PROP_PROGRESS:
      mx_slider_set_progress (self, g_value_get_double (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
mx_slider_dispose (GObject *object)
{
  MxSlider *self = MX_SLIDER (object);
  MxSliderPrivate *priv = self->priv;

  if (priv->capture_handler && priv->trough)
    {
      ClutterActor *stage;

      stage = clutter_actor_get_stage (priv->trough);
      g_signal_handler_disconnect (stage, priv->capture_handler);
      priv->capture_handler = 0;
    }

  if (priv->trough_bg)
    {
      clutter_actor_unparent (priv->trough_bg);
      priv->trough_bg = NULL;
    }

  if (priv->fill)
    {
      clutter_actor_unparent (priv->fill);
      priv->fill = NULL;
    }

  /* unparent the handle before the trough, as the handle is parented on the
   * trough */
  if (priv->handle)
    {
      clutter_actor_unparent (priv->handle);
      priv->handle = NULL;
    }

  if (priv->trough)
    {
      clutter_actor_unparent (priv->trough);
      priv->trough = NULL;
    }

  G_OBJECT_CLASS (mx_slider_parent_class)->dispose (object);
}

static void
mx_slider_class_init (MxSliderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  ClutterActorClass *actor_class = CLUTTER_ACTOR_CLASS (klass);
  GParamSpec *pspec;

  g_type_class_add_private (klass, sizeof (MxSliderPrivate));

  object_class->get_property = mx_slider_get_property;
  object_class->set_property = mx_slider_set_property;
  object_class->dispose = mx_slider_dispose;

  actor_class->paint = mx_slider_paint;
  actor_class->pick = mx_slider_pick;
  actor_class->get_preferred_width = mx_slider_get_preferred_width;
  actor_class->get_preferred_height = mx_slider_get_preferred_height;
  actor_class->allocate = mx_slider_allocate;
  actor_class->map = mx_slider_map;
  actor_class->unmap = mx_slider_unmap;

  pspec = g_param_spec_double ("progress",
                               "Progress",
                               "Progress",
                               0.0, 1.0, 0.0, G_PARAM_READWRITE);
  g_object_class_install_property (object_class, PROP_PROGRESS, pspec);
}

static void
mx_slider_style_changed_cb (MxSlider *self)
{
  gboolean relayout;
  gint trough_height;
  guint handle_width, handle_height;

  MxSliderPrivate *priv = self->priv;

  mx_stylable_get (MX_STYLABLE (self),
                   "x-mx-trough-height", &trough_height,
                   "x-mx-handle-width",  &handle_width,
                   "x-mx-handle-height", &handle_height,
                   NULL);

  relayout = FALSE;

  if (priv->trough_height != trough_height)
    {
      priv->trough_height = trough_height;
      relayout = TRUE;
    }

  if (priv->handle_width != handle_width)
    {
      priv->handle_width = handle_width;
      relayout = TRUE;
    }

  if (priv->handle_height != handle_height)
    {
      priv->handle_height = handle_height;
      relayout = TRUE;
    }

  if (relayout)
    clutter_actor_queue_relayout (CLUTTER_ACTOR (self));
}

static void
mx_slider_init (MxSlider *self)
{
  MxSliderPrivate *priv;

  self->priv = priv = SLIDER_PRIVATE (self);

  g_signal_connect (self, "style-changed",
                    G_CALLBACK (mx_slider_style_changed_cb), NULL);

  priv->trough_bg = CLUTTER_ACTOR (_mx_progress_bar_fill_new ());
  clutter_actor_set_name (priv->trough_bg, "trough-background");
  clutter_actor_set_reactive (priv->trough_bg, TRUE);
  clutter_actor_set_parent (priv->trough_bg, CLUTTER_ACTOR (self));
  g_signal_connect (priv->trough_bg, "button-press-event",
                    G_CALLBACK (on_trough_bg_button_press_event), self);
  g_signal_connect (priv->trough_bg, "button-release-event",
                    G_CALLBACK (on_trough_bg_button_release_event), self);
  g_signal_connect (priv->trough_bg, "leave-event",
                    G_CALLBACK (on_trough_bg_leave_event), self);

  priv->fill = CLUTTER_ACTOR (_mx_progress_bar_fill_new ());
  clutter_actor_set_name (priv->fill, "fill");
  clutter_actor_set_parent (priv->fill, CLUTTER_ACTOR (self));

  priv->trough = CLUTTER_ACTOR (mx_frame_new ());
  clutter_actor_set_name (priv->trough, "trough");
  clutter_actor_set_parent (priv->trough, CLUTTER_ACTOR (self));

  self->priv->handle = CLUTTER_ACTOR (mx_button_new ());
  clutter_actor_set_name (priv->handle, "handle");
  clutter_actor_set_parent (priv->handle, priv->trough);
  g_signal_connect (priv->handle, "button-press-event",
                    G_CALLBACK (on_handle_button_press_event), self);
}

/**
 * mx_slider_new:
 *
 * Create a new slider
 *
 * Returns: a new #MxSlider
 */
ClutterActor *
mx_slider_new (void)
{
  return g_object_new (MX_TYPE_SLIDER, NULL);
}

/**
 * mx_slider_set_progress:
 * @bar: A #MxProgressBar
 * @progress: A value between 0.0 and 1.0
 *
 * Set the progress of the slider
 */
void
mx_slider_set_progress (MxSlider *bar,
                        gdouble      progress)
{
  MxSliderPrivate *priv = bar->priv;

  g_return_if_fail (MX_IS_SLIDER (bar));

  if (priv->progress == progress)
    return;

  if (G_UNLIKELY ((progress < 0.0) || (progress > 1.0)))
    {
      g_warning ("progress must be a number between 0.0 and 1.0");
      return;
    }

  priv->progress = progress;

  mx_slider_allocate_fill_handle (bar, NULL, 0);
  clutter_actor_queue_redraw (CLUTTER_ACTOR (bar));

  g_object_notify (G_OBJECT (bar), "progress");
}

/**
 * mx_slider_get_progress:
 * @bar: A #MxProgressBar
 *
 * Retrieve the current progress of the media bar
 *
 * Returns: gdouble
 */
gdouble
mx_slider_get_progress (MxSlider *bar)
{
  g_return_val_if_fail (MX_IS_SLIDER (bar), 0.0);

  return bar->priv->progress;
}
