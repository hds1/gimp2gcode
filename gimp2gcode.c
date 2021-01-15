/* GIMP - The GNU Image Manipulation Program
 * Copyright (C) 1995 Spencer Kimball and Peter Mattis
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

/*

  Tested with Xubuntu 20.04, Gimp 2.10.18, grbl 1.1

  Build:
  Copy to folder and run "gimptool-2.0 --install gimp2gcode.c"
  This will install the binary in your local ~/.config/gimp folder.

  This plugin sholud compile under Gimp3, but this has not been tested.
  Also no tests on Windows or Macs.

  The plugin comes with some default settings for GRBL. Settings can be changed and saved to your HOMEDIR, gimp2gcode.conf

  Created NGC files will also be saved to your HOMEDIR.

  Prepare picture:
  - Convert to 8Bit grayscale
  - Remove Alphachannel
  - Apply Menu -> "Filter - Misc - gimp2gcode"
*/

#undef DEBUG

#include <errno.h>
#include <string.h>

#include <gimp-2.0/libgimp/gimp.h>
#include <gimp-2.0/libgimp/gimpui.h>

#include <glib-2.0/glib/gstdio.h>

#define PLUG_IN_BINARY "gimp2gcode"
#define PLUG_IN_ROLE   "gimp2gcode"
#define PLUG_IN_PROC   "gimp2gcode"

#define BASE_SETTINGS "\
[settings]\n\
# absolute coordinates\n\
abscoords = G90\n\
# set units to mm\n\
dimension = G21\n\
# Laser on with dynamics (grbl 1.1)\n\
laseron   = M4\n\
# Laser off\n\
laseroff  = M5\n\
# Laser Max PWM\n\
lasermax  = 400\n\
# Laser Min PWM\n\
lasermin  = 50\n\
# Base speed [mm/min]\n\
speed     = F1500\n\
# Laser Width [mm]\n\
width     = 0.15\n\
# NGC filename\n\
outfilename = gimp2gcode.ngc\n\
"

#define CONFIG_FILE "gimp2gcode.conf"

static void query (void);
static void run   (const gchar      *name,
                   gint              nparams,
                   const GimpParam  *param,
                   gint             *nreturn_vals,
                   GimpParam       **return_vals);

static gint     gimp2gcode_dialog  (void);
static gboolean gimp2gcode_read_configfile (gchar *);
static gboolean gimp2gcode_save_configfile (gchar *);
static gboolean gimp2gcode_create_gcode (gint32, gint32, gchar *);
static void     menu_entry_callback (GtkWidget *widget,
                                     gpointer   data );
static GtkWidget * make_menu_entry (gchar * my_label, gchar * my_value);

gchar             **keys_array;
GKeyFile          *keyfile;
gsize             num_keys;
const gchar       *homedir;

GimpPlugInInfo PLUG_IN_INFO =
{
        NULL,
        NULL,
        query,
        run
};

MAIN()

        static void
        query (void)
{
        static GimpParamDef args[] =
                {
                        {
                                GIMP_PDB_INT32,
                                "run-mode",
                                "Run mode"
                        },
                        {
                                GIMP_PDB_IMAGE,
                                "image",
                                "Input image"
                        },
                        {
                                GIMP_PDB_DRAWABLE,
                                "drawable",
                                "Input drawable"
                        }
                };

        gimp_install_procedure (
                "gimp2gcode",
                "gimp2gcode",
                "convert 8Bit BW to gcode (grbl 1.1)",
                "Heiko Schroeter",
                "Copyright Heiko Schroeter",
                "2021",
                "_gimp2gcode...",
                "GRAY",
                GIMP_PLUGIN,
                G_N_ELEMENTS (args), 0,
                args, NULL);

        gimp_plugin_menu_register (PLUG_IN_PROC,
                                   "<Image>/Filters/Misc");
}

static gboolean
gimp2gcode_save_configfile(gchar * fqn_configfile)
{
        GError  *error = NULL;

        g_key_file_save_to_file (keyfile, fqn_configfile, &error);
        g_message("Config Saved\n");
        if(! error)
                return TRUE;
        return FALSE;
}

static gboolean
gimp2gcode_read_configfile(gchar * fqn_configfile)
{
        GKeyFileFlags flags;
        GError        *error = NULL;

        keyfile  = g_key_file_new ();
        flags    = G_KEY_FILE_KEEP_TRANSLATIONS | G_KEY_FILE_KEEP_COMMENTS;

        g_key_file_load_from_file (keyfile, fqn_configfile, flags, &error);
        if(error) goto fehler;

        keys_array = g_key_file_get_keys (keyfile, "settings", &num_keys, NULL);
        //g_message("Config Read\n");
        return TRUE;

fehler:
        //g_message("Config NOT Read\n");
        return FALSE;
}

static void
run (const gchar      *name,
     gint              nparams,
     const GimpParam  *param,
     gint             *nreturn_vals,
     GimpParam       **return_vals)
{
        FILE *fptr;
        static GimpParam  values[1];
        GimpPDBStatusType status = GIMP_PDB_SUCCESS;
        GimpRunMode       run_mode;
        gint32            drawable_id;
        gint32            image_id;
        gboolean          result = FALSE;
        GError            *err;
        gchar             *fqn_configfile;

        /* Set config file name in HOMEDIR */
        homedir = g_getenv ("HOME");

        if (!homedir)
                homedir = g_get_home_dir ();

        fqn_configfile = g_strjoin ("/",
                                    homedir,
                                    CONFIG_FILE,
                                    NULL);
        /* Try to read config if exists */
        gimp2gcode_read_configfile(fqn_configfile);

        /* If not existing create default */
        if (!g_file_test (fqn_configfile, G_FILE_TEST_EXISTS))
        {
                g_file_set_contents (fqn_configfile,
                                     BASE_SETTINGS,
                                     -1,
                                     NULL);
                gimp2gcode_read_configfile(fqn_configfile);
                g_remove(fqn_configfile);
        }

        /* Init Gegl and Babl */
        gegl_init (NULL, NULL);

        /* Setting mandatory output values */
        *nreturn_vals = 1;
        *return_vals  = values;

        values[0].type = GIMP_PDB_STATUS;
        values[0].data.d_status = status;
        /* values[0].data.d_status = GIMP_PDB_CALLING_ERROR; */

        run_mode      = param[0].data.d_int32;
        image_id      = param[1].data.d_int32;
        drawable_id   = param[2].data.d_int32;

        switch (gimp2gcode_dialog ())
        {
        case GTK_RESPONSE_APPLY:
                gimp2gcode_save_configfile (fqn_configfile);
                break;
        case GTK_RESPONSE_OK:
                gimp2gcode_create_gcode (image_id,
                                         drawable_id,
                                         g_key_file_get_value (keyfile, "settings",
                                                               "outfilename", NULL));
                break;
        case GTK_RESPONSE_CANCEL:
        default:
                break;
        }

        values[0].data.d_status = status;
}

static void menu_entry_callback( GtkWidget *widget,
                                 gpointer   data )
{
        /* Callback: Save changed text entries in the keyfile */
        gchar *my_label = g_object_get_data(G_OBJECT(widget), "my_label");
        const gchar *my_value = gtk_entry_get_text(GTK_ENTRY (widget));

        g_key_file_set_string (keyfile, "settings", my_label, my_value);

        /* g_message("### %s: %s\n", */
        /*          my_label, */
        /*          g_key_file_get_value (keyfile, "settings", */
        /*                                my_label, NULL)); */
}

static GtkWidget * make_menu_entry (gchar * my_label, gchar * my_value)
{
        GtkWidget *widget;
        GtkWidget *local_hbox;

        local_hbox = gtk_hbox_new (FALSE, 0);

        widget = gtk_label_new_with_mnemonic (my_label);
        gtk_container_add (GTK_CONTAINER (local_hbox), widget);

        widget = gtk_entry_new ();
        g_object_set_data(G_OBJECT(widget), "my_label", my_label);
        g_object_set_data(G_OBJECT(widget), "my_value", my_value);
        gtk_entry_set_text (GTK_ENTRY (widget), my_value);
        g_signal_connect (widget, "changed",
                          G_CALLBACK (menu_entry_callback), NULL);
        gtk_container_add (GTK_CONTAINER (local_hbox), widget);

        g_key_file_set_string (keyfile, "settings", my_label, my_value);

        return local_hbox;
}


static gint
gimp2gcode_dialog (void)
{
        GtkWidget *dialog;
        GtkWidget *main_vbox;
        GtkWidget *vbox_entry;

        GError    *error = NULL;
        gint      dialogrun;

        gint      i;

        gimp_ui_init (PLUG_IN_BINARY, TRUE);

        dialog = gimp_dialog_new (("GRBL (1.1) Settings"), PLUG_IN_ROLE,
                                  NULL, 0,
                                  NULL, NULL,
                                  ("_Save Config"), GTK_RESPONSE_APPLY,
                                  ("_Cancel"), GTK_RESPONSE_CANCEL,
                                  ("_OK"),     GTK_RESPONSE_OK,
                                  NULL);

        gtk_dialog_set_alternative_button_order (GTK_DIALOG (dialog),
                                                 GTK_RESPONSE_OK,
                                                 GTK_RESPONSE_CANCEL,
                                                 -1);

        gimp_window_set_transient (GTK_WINDOW (dialog));




        main_vbox = gtk_vbox_new (FALSE, 0);
        gtk_container_add (GTK_CONTAINER (GTK_DIALOG(dialog)->vbox), main_vbox);

        for(i = 0; i < num_keys ;i++)
        {
                vbox_entry = make_menu_entry (keys_array[i],
                                          g_key_file_get_value (keyfile, "settings",
                                                                keys_array[i], NULL));
                gtk_container_add (GTK_CONTAINER (main_vbox), vbox_entry);
                gtk_widget_show (vbox_entry);
        }




        gtk_widget_show_all(dialog);

        //dialogrun = (gimp_dialog_run (GIMP_DIALOG (dialog)) == GTK_RESPONSE_OK);
        dialogrun = gimp_dialog_run (GIMP_DIALOG (dialog));

        gtk_widget_destroy (dialog);

        return dialogrun;
}


static gboolean
gimp2gcode_create_gcode (gint32 image_id, gint32 drawable_id, gchar * outfilename)
{
        GeglBuffer    *drawable;
        const Babl    *format;
        FILE          *fptr;
        gchar         *fqn_gcodefile;
        guchar        *line = NULL;   /* Pixel data */
        gint          x, y, width, height, col;
        gint          lastx = -1, lasty = -1;
        gint          byteperpx, bpp, px_level;
        gint          lasermax = 10;
        gint          lasermin =  2;
        gint          laserpower = 0, lastlaserpower = 0;
        gfloat        laserwidth;
        gboolean      lineend = FALSE, forward = TRUE;

        drawable = gimp_drawable_get_buffer (drawable_id);
        format   = gimp_drawable_get_format (drawable_id);
        bpp      = gimp_drawable_bpp (drawable_id);

        fqn_gcodefile = g_strjoin ("/",
                                   homedir,
                                   outfilename,
                                   NULL);

        width  = gegl_buffer_get_width (drawable);
        height = gegl_buffer_get_height (drawable);


        /* Generate gcode */
        line   = g_try_malloc (width * bpp + bpp);

        if (! line)
                goto fehler;

        byteperpx = babl_format_get_bytes_per_pixel (format);

        fptr = g_fopen (fqn_gcodefile, "w+");

        if (! fptr)
                goto fehler;

        lasermax = g_key_file_get_integer (keyfile,
                                           "settings",
                                           "lasermax",
                                           NULL);
        lasermin = g_key_file_get_integer (keyfile,
                                           "settings",
                                           "lasermin",
                                           NULL);
        laserwidth = g_key_file_get_double (keyfile,
                                            "settings",
                                            "width",
                                            NULL);

        gimp_progress_init_printf (("Opening '%s'"),
                                   gimp_filename_to_utf8 (fqn_gcodefile));
        /* Preamble  */
        fprintf (fptr, "; gimp2gcode for Laser (grbl 1.1)\n; Jan 2021 Heiko Schroeter\n");
        fprintf (fptr, "; Width: %d [px], Height: %d [px]\n", width, height);
        fprintf (fptr, "; Laserwidth: %0.2f [mm]\n", laserwidth);
        fprintf (fptr, "; {Width[mm] = laserwidth * PXwidth, Height[mm] = laserwidth * PXheight}\n");
        fprintf (fptr, "; Width: %0.1f [mm], Height: %0.1f [mm]\n",
                 laserwidth * (gfloat)width,
                 laserwidth * (gfloat)height);
        fprintf (fptr, "; Bottom Left corner of pic is Pos 0|0 for the laser.\n");
        fprintf (fptr, "; Laser plot direction is picture bottom up.\n");
        fprintf (fptr, "; File: %s\n", gimp_image_get_filename (image_id));
        fprintf (fptr, ";\n;\n");

        /* Gcode for laser */
        fprintf (fptr, "%s ; Set units to mm\n",
                 g_key_file_get_value (keyfile, "settings", "dimension", NULL));
        fprintf (fptr, "%s ; Use absolute coordinates\n",
                 g_key_file_get_value (keyfile, "settings", "abscoords", NULL));
        fprintf (fptr, "S0  ; Power off laser i.e. PWM=0\n");
        fprintf (fptr, "%s  ; Activate Laser with dynamics\n",
                 g_key_file_get_value (keyfile, "settings", "laseron", NULL));
        fprintf (fptr, "%s ; Set speed\n",
                 g_key_file_get_value (keyfile, "settings", "speed", NULL));
        fprintf (fptr, ";\n;\n");
        /* Pixel processing.
           Gimp coords:
           top left corner:  0 0
           bottom right:     width height
         */
        fprintf (fptr, "G00 X0 Y0 S0\n");
        for (y = height - 1; y >= 0; y--)
        {
                fprintf (fptr,";--%s--\n", forward == TRUE ? ">":"<");

//                lastlaserpower = 0;
                lineend = FALSE;
                /* get px col */
                gegl_buffer_get (drawable, GEGL_RECTANGLE (0, y, width, 1),
                                 1.0, format, line,
                                 GEGL_AUTO_ROWSTRIDE, GEGL_ABYSS_NONE);

                line[width * bpp] = 255; /* Set Overscan px to white */

                for (x = 0; x <= width * bpp; x++)
                {
                        if(forward)
                                col = x;
                        else
                                col = width - x;

                        px_level   = 255 - line[col];
                        laserpower = (gint) ((gfloat)lasermin + ((gfloat)(lasermax - lasermin)) / 255.0 * (gfloat)px_level);

                        if (x >= width * bpp)
                                lineend = TRUE;

                        if(((col > 0) && (laserpower != lastlaserpower)) || lineend)
                        {
                                /* powerlevel changed or end of scanline reached */
                                fprintf(fptr,"G01 X%0.2f Y%0.2f S%d\n",
                                        (float)col * laserwidth,
                                        (float)(height - y -1) * laserwidth,
                                        lastlaserpower);
                        }
                        if (lineend && y > 0)
                        {
                                /* Move one Scanline upwards without power */
                                fprintf(fptr,"G01 X%0.2f Y%0.2f S%d ;u\n",
                                        (float)col * laserwidth,
                                        (float)(height - y) * laserwidth,
                                        lasermin);
                        }
                        lastlaserpower = laserpower;
                }
                forward = !(forward);
                gimp_progress_update ((float) (height - y) / (float) height);
        }

        fprintf (fptr, ";\n;\n");
        fprintf (fptr, "%s ; Laser Off\n",
                 g_key_file_get_value (keyfile, "settings", "laseroff", NULL));
        fprintf (fptr, "G00 X0 Y0 S0 ; romanus eunt domus ... \n");

        fclose(fptr);

        gimp_progress_update (1.0);

        g_message ("Creating Gcode\nimageID: %d\ndrawableID: %d\nWidth: %d\nHeight: %d\nByte per Px: %d\n%s\n",
                   image_id, drawable_id, width, height, byteperpx, fqn_gcodefile);

        g_free (line);
        g_object_unref (drawable);

        return TRUE;
fehler:
        return FALSE;
}
