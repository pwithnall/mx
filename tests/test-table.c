/*
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
 */
#include <stdlib.h>
#include <stdio.h>

#include <glib.h>

#include <clutter/clutter.h>
#include <nbtk/nbtk.h>

static void
toggle_expand (NbtkButton *button, ClutterContainer *table)
{
  gboolean x_expand;
  gchar *label;


  clutter_container_child_get (table, CLUTTER_ACTOR (button),
                               "x-expand", &x_expand,
                               NULL);

  x_expand = !x_expand;

  clutter_container_child_set (table, CLUTTER_ACTOR (button),
                               "x-expand", x_expand,
                               "y-expand", x_expand,
                               NULL);

  label = g_strdup_printf ("Expand = %d", x_expand);
  nbtk_button_set_label (button, label);

  g_free (label);
}

static void
randomise_align (NbtkButton *button, ClutterContainer *table)
{
  gdouble x_align, y_align;
  gchar *label;
  
  x_align = g_random_double ();
  y_align = g_random_double ();
  
  clutter_container_child_set (table, CLUTTER_ACTOR (button),
                               "x-align", x_align,
                               "y-align", y_align,
                               NULL);

  label = g_strdup_printf ("Align (%.2lf, %.2lf)", x_align, y_align);
  nbtk_button_set_label (button, label);
  g_free (label);
}

static void
stage_size_notify_cb (ClutterActor *stage,
                      GParamSpec *pspec,
                      ClutterActor *table)
{
  gfloat width, height;

  clutter_actor_get_size (stage, &width, &height);
  clutter_actor_set_size (table, width - 10, height - 10);
}

static void
toggle_visible (NbtkButton *button)
{
  clutter_actor_hide (CLUTTER_ACTOR (button));
}

int
main (int argc, char *argv[])
{
  ClutterActor *stage, *button2;
  NbtkWidget *table;
  NbtkWidget *button1, *button3, *button4, *button5,
             *button6, *button7, *button8, *button9,
             *button10;

  clutter_init (&argc, &argv);

  /* load the style sheet */
  nbtk_style_load_from_file (nbtk_style_get_default (),
                             "style/default.css", NULL);

  stage = clutter_stage_get_default ();
  clutter_stage_set_user_resizable (CLUTTER_STAGE (stage), TRUE);

  table = nbtk_table_new ();
  nbtk_table_set_col_spacing (NBTK_TABLE (table), 10);
  nbtk_table_set_row_spacing (NBTK_TABLE (table), 10);


  g_signal_connect (stage, "notify::width",
                    G_CALLBACK (stage_size_notify_cb), table);
  g_signal_connect (stage, "notify::height",
                    G_CALLBACK (stage_size_notify_cb), table);

  button1 = nbtk_button_new_with_label ("button1");
  button2 = clutter_texture_new_from_file ("redhand.png", NULL);
  button3 = nbtk_button_new_with_label ("button3");
  button4 = nbtk_button_new_with_label ("Expand = 1");
  button5 = nbtk_button_new_with_label ("button5");
  button6 = nbtk_button_new_with_label ("button6");
  button7 = nbtk_button_new_with_label ("Align (0.50, 0.50)");
  button8 = nbtk_button_new_with_label ("button8");
  button9 = nbtk_button_new_with_label ("button9");
  button10 = nbtk_button_new_with_label ("button10");

  nbtk_table_add_actor (NBTK_TABLE (table), CLUTTER_ACTOR (button1), 0, 0);
  nbtk_table_add_actor (NBTK_TABLE (table), button2, 0, 1);
  nbtk_table_add_actor (NBTK_TABLE (table), CLUTTER_ACTOR (button3), 1, 1);
  nbtk_table_add_actor (NBTK_TABLE (table), CLUTTER_ACTOR (button4), 2, 0);
  nbtk_table_add_actor (NBTK_TABLE (table), CLUTTER_ACTOR (button5), 3, 0);
  nbtk_table_add_actor (NBTK_TABLE (table), CLUTTER_ACTOR (button6), 3, 1);
  nbtk_table_add_actor (NBTK_TABLE (table), CLUTTER_ACTOR (button7), 4, 1);
  nbtk_table_add_actor (NBTK_TABLE (table), CLUTTER_ACTOR (button8), 4, 0);
  nbtk_table_add_actor (NBTK_TABLE (table), CLUTTER_ACTOR (button9), 5, 0);
  nbtk_table_add_actor (NBTK_TABLE (table), CLUTTER_ACTOR (button10), -1, 0);
  nbtk_table_child_set_row_span (NBTK_TABLE (table), CLUTTER_ACTOR (button1), 2);
  nbtk_table_child_set_row_span (NBTK_TABLE (table), CLUTTER_ACTOR (button7), 2);
  nbtk_table_child_set_col_span (NBTK_TABLE (table), CLUTTER_ACTOR (button4), 2);


  clutter_actor_set_size (CLUTTER_ACTOR (button1), 100, 100);

  clutter_container_child_set (CLUTTER_CONTAINER (table),
                               CLUTTER_ACTOR (button1),
                               "x-expand", FALSE, "y-expand", FALSE,
                               NULL);
  clutter_container_child_set (CLUTTER_CONTAINER (table),
                               CLUTTER_ACTOR (button5),
                               "x-expand", FALSE, "y-expand", FALSE,
                               NULL);
  clutter_container_child_set (CLUTTER_CONTAINER (table),
                               CLUTTER_ACTOR (button7),
                               "x-expand", TRUE, "y-expand", TRUE,
                               "x-fill", FALSE, "y-fill", FALSE,
                               NULL);
  clutter_container_child_set (CLUTTER_CONTAINER (table),
                               CLUTTER_ACTOR (button8),
                               "x-expand", FALSE, "y-expand", FALSE,
                               NULL);
  clutter_container_child_set (CLUTTER_CONTAINER (table),
                               CLUTTER_ACTOR (button9),
                               "x-expand", FALSE, "y-expand", FALSE,
                               NULL);

  g_object_set (G_OBJECT (button2), "keep-aspect-ratio", TRUE, NULL);
  clutter_container_child_set (CLUTTER_CONTAINER (table),
                               CLUTTER_ACTOR (button2),
                               "y-fill", FALSE,
                               "x-fill", FALSE,
                               NULL);

  clutter_container_child_set (CLUTTER_CONTAINER (table),
                               CLUTTER_ACTOR (button10),
                               "allocate-hidden", FALSE,
                               NULL);

  clutter_container_add_actor (CLUTTER_CONTAINER (stage), CLUTTER_ACTOR (table));

  clutter_actor_set_position (CLUTTER_ACTOR (table), 5, 5);

  g_signal_connect (button4, "clicked", G_CALLBACK (toggle_expand), table);
  g_signal_connect (button7, "clicked", G_CALLBACK (randomise_align), table);
  g_signal_connect (button10, "clicked", G_CALLBACK (toggle_visible), NULL);

  clutter_actor_show (stage);

  g_debug ("table row count = %d",
           nbtk_table_get_row_count (NBTK_TABLE (table)));
  g_debug ("table column count = %d",
           nbtk_table_get_column_count (NBTK_TABLE (table)));

  clutter_main ();

  return EXIT_SUCCESS;
}
