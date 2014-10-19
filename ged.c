/*
  Calculator - a lightweight GTK text editor
  Copyright (C) 2005 Ben Good, James Hartzell
  This program is free software; you can redistribute it and/or modify it 
  under the terms of the GNU General Public License as published by the
  Free Software Foundation; either version 2 of the Licesnse, or (at your
  option) any later version.
  This program is distributed in the hope that it will be useful but 
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. 
  See the GNU General Public License for more details. You should have
  received a copy of the GNU General Public License along with this 
  program; if not, write to the Free Software Foundaton, Inc.,
  59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
*/

/*Standard C library includes*/
#include <stdarg.h>
#include <stdio.h>

/*library includes*/
#include <gtk/gtk.h>

/*main structure - this is not supposed to be an gobject-related anything!*/
gint ged_instances=0;
typedef struct _GedWindow
{
  GtkWidget *window;
  GtkWidget *close_dialog; /*should this be function-scope?*/
  GtkWidget *file_selector;
  GtkWidget *text_view;
  char *current_filename; /*should only be set by access function*/
  gboolean closing;
  GtkAccelGroup *accel_group;
} GedWindow;
/*public function declarations*/
GtkWidget *create_menu(GedWindow *self,const char *name,...);
void set_current_filename(GedWindow *self,const char *new);
void do_quit(GedWindow *self);
void open_file(GedWindow *self,const char *filename);
void save_file(GedWindow *self,const char *filename);
gchar *get_buffer_text(GedWindow *self);
GtkAccelGroup *current_accel_group(GedWindow *self);
GedWindow *draw_ged_window();

/*callback function declarations*/
void close_dialog_answered(GtkDialog *dialog,gint id,gpointer data);
gboolean user_quit(GtkWidget *window,
		   GdkEvent *event, gpointer user_data);
void new_item_clicked(GtkMenuItem *,gpointer);
void open_item_clicked(GtkMenuItem *,gpointer);
void open_selected_file(GtkButton *,gpointer);
void save_item_clicked(GtkMenuItem *,gpointer);
void save_as_item_clicked(GtkMenuItem *,gpointer);
void save_selected_file(GtkButton *,gpointer);
void quit_item_clicked(GtkMenuItem *,gpointer);
void cut_item_clicked(GtkMenuItem *,gpointer);
void copy_item_clicked(GtkMenuItem *,gpointer);
void paste_item_clicked(GtkMenuItem *,gpointer);
void word_wrap_item_clicked(GtkMenuItem *,gpointer);

/*our actual function definitions:*/

/* create_menu-
	returns an item named name with the menu. Each item is specified
	by name, accel, function*/
typedef void (*menu_function)(GtkMenuItem*,gpointer);
GtkWidget *create_menu(GedWindow *self,const char *name, ...)
{
  GtkWidget *menu,*item;
  const char *item_name,*accelerator;
  guint accel_key;
  GdkModifierType accel_mod;
  va_list ap;
  menu = gtk_menu_new();
  gtk_menu_set_accel_group(GTK_MENU(menu),current_accel_group(self));
  va_start(ap,name);
  while(item_name=va_arg(ap,const char *))
    {
      item=gtk_menu_item_new_with_mnemonic(item_name);
      if(accelerator=va_arg(ap,const char*))
	  {
	    gtk_accelerator_parse(accelerator,&accel_key,&accel_mod);
	    gtk_widget_add_accelerator(item,"activate",
				       current_accel_group(self),
				       accel_key,accel_mod,
				       GTK_ACCEL_VISIBLE);
	  }
      g_signal_connect(item,"activate",
		       G_CALLBACK(va_arg(ap,menu_function)),
		       (void *)self);
      gtk_menu_append(GTK_MENU(menu),item);
      gtk_widget_show(item);
    }
  item=gtk_menu_item_new_with_mnemonic(name);
  gtk_widget_show(item);
  gtk_menu_item_set_submenu(GTK_MENU_ITEM(item),menu);
  return item;
}

/*current_accel_group -
  since we only have one window, we only need one accelerator 
  group, so we use this function to keep track of the shared instance.
  This does use the reference pointer memory model rather than
  the widget memory model - luckily, it only needs freed when
  we're about to exit anyway*/
GtkAccelGroup *current_accel_group(GedWindow *self)
{
  if(!self->accel_group)
    self->accel_group=gtk_accel_group_new();
  return self->accel_group;
}

/*set_current_filename -
  sets the current_filename variable using memory management
  functions*/
void set_current_filename(GedWindow *self,const char *new)
{
  g_free(self->current_filename);
  if(new!=NULL)
    {
      self->current_filename=g_malloc(strlen(new)+1);
      strcpy(self->current_filename,new);
      gtk_window_set_title(GTK_WINDOW(self->window),self->current_filename);
    }
  else
    self->current_filename=0;
}

/*open_file -
  Reads a file into the buffer and makes it the current file*/
void open_file(GedWindow *self,const char *filename)
{
  FILE *infile;
  GtkTextBuffer *buffer;
  char *data=0;
  size_t data_len=0;
  size_t data_offset=0;
  size_t read_len=0;
  buffer=gtk_text_view_get_buffer(GTK_TEXT_VIEW(self->text_view));
  if(gtk_text_buffer_get_modified(buffer) || self->current_filename)
    {
      open_file(draw_ged_window(),filename);
      return;
    }
  set_current_filename(self,filename);
  if(!(infile = fopen(filename,"r")))
    return; /*it still changes the current filename, so save will create
	      a new file. this allows a new file to be created with ged on
	      the command line. you probably don't want to use that
	      feature in the GUI*/
  data=malloc(1); /*null value crashes gtk+, even if it's empty*/
  while(!feof(infile))
  {
    data_len+=512;
    data=g_realloc(data,data_len);
    read_len=fread(data+data_offset,1,512,infile);
    if(read_len!=512)
      {
	data_len=data_len-512+read_len;
	data=g_realloc(data,data_len+1); /*always add one byte so it's never
					   completely freed.*/
      }
    data_offset+=read_len;
  }
  fclose(infile);
  data[data_len]='\0';
  buffer=gtk_text_view_get_buffer(GTK_TEXT_VIEW(self->text_view));
  gtk_text_buffer_set_text(buffer,data,data_len);
  gtk_text_buffer_set_modified(buffer,0);
}

gchar *get_buffer_text(GedWindow *self)
{
  GtkTextIter start,end;
  GtkTextBuffer *buffer;
  buffer=gtk_text_view_get_buffer(GTK_TEXT_VIEW(self->text_view));
  gtk_text_buffer_get_start_iter(buffer,&start);
  gtk_text_buffer_get_end_iter(buffer,&end);
  return gtk_text_buffer_get_text(buffer,&start,&end,0);
}

void save_file(GedWindow *self,const char *filename)
{
  FILE *outfile;
  gchar *text,*text_remaining;
  GtkTextBuffer *buf;
  outfile=fopen(filename,"w");
  text=get_buffer_text(self);
  fprintf(outfile,"%s\n",text); /*some programs need that last newline*/
  g_free(text);
  /*if this was because of "Save" after asking if we want to save before
    closing, we should close now*/
  if(self->closing)
    do_quit(self);
  else
    {
      /*it hasn't been modified since the last save and won't need saving 
	when we quit*/
      buf=gtk_text_view_get_buffer(GTK_TEXT_VIEW(self->text_view));
      gtk_text_buffer_set_modified(buf,0);
      /*and the current filename should be what we just saved to*/
      set_current_filename(self,filename);
    }
}

int main(int argc, char **argv)
{
  int i;
  gtk_init(&argc,&argv);
  for(i=1;i<argc;i++)
    open_file(draw_ged_window(),argv[i]);
  if(argc==1)
    draw_ged_window();
  gtk_main();
  return 0;
}

GedWindow *draw_ged_window()
{
  GtkWidget *container,*menu_bar,*text_area;

  GedWindow *self;
  self=g_malloc(sizeof(GedWindow)); /*g_malloc0 will not work here, because*/
  self->current_filename=0;         /*the C standard says that although the*/
  self->closing=0;                  /*constant 0 means NULL for pointers,*/
  self->file_selector=0;            /*that does not mean every bit of that*/
  self->accel_group=0;              /*0 pointer must be zero :). On most*/
  self->close_dialog=0;             /*archs it'll work anyway, but you
				      shouldn't risk it*/
  
  container = gtk_vbox_new(0,0);
  
  menu_bar=gtk_menu_bar_new();
  gtk_widget_show(menu_bar);
  gtk_menu_bar_append(menu_bar,
		      create_menu(self,"_File",
				  "_New","<Control>n",&new_item_clicked,
				  "_Open...","<Control>o",&open_item_clicked,
				  "_Save","<Control>s",&save_item_clicked,
				  "Save _as...",NULL,&save_as_item_clicked,
				  "_Quit","<Control>q",&quit_item_clicked,
				  NULL));
  gtk_menu_bar_append(menu_bar,
		      create_menu(self,"_Edit",
				  "Cu_t","<Control>x",&cut_item_clicked,
				  "_Copy","<Control>c",&copy_item_clicked,
				  "_Paste","<Control>v",&paste_item_clicked,
				  /* "Disable _Word Wrap",NULL,
				     &word_wrap_item_clicked, */
				  NULL)); 
  /*TODO: word wrap feature should be check-box-type item*/

  self->text_view=gtk_text_view_new();
  gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(self->text_view),GTK_WRAP_WORD);
  gtk_widget_show(self->text_view);
  
  text_area=gtk_scrolled_window_new(NULL,NULL);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(text_area),
				 GTK_POLICY_AUTOMATIC,GTK_POLICY_AUTOMATIC);
  gtk_container_add(GTK_CONTAINER(text_area),self->text_view);
  gtk_widget_show(text_area);
  
  container=gtk_vbox_new(0,0);
  gtk_box_pack_start(GTK_BOX(container),menu_bar,0,0,0);
  gtk_box_pack_start(GTK_BOX(container),text_area,1,1,0);
  gtk_widget_show(container);
  
  self->window=gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_container_add(GTK_CONTAINER(self->window),container);
  gtk_window_set_title(GTK_WINDOW(self->window),"Untitled");
  /*gtk_window_set_border_width(GTK_WINDOW(self->window),0); 
    /*apparently there's no
    such function*/
  gtk_window_set_default_size(GTK_WINDOW(self->window),500,400);
  g_signal_connect(self->window,"delete_event",G_CALLBACK(user_quit),
                   (void *)self);
  gtk_widget_show(self->window);
  ged_instances++;
  return self;
}

void do_quit(GedWindow *self)
{
  gtk_widget_destroy(self->window);
  if(!--ged_instances)
    gtk_main_quit();
  g_free(self);
}

gboolean user_quit(GtkWidget *window, GdkEvent *event, gpointer data)
{	
  gint result;
  gboolean modified;
  GtkTextBuffer *buffer;
  GedWindow *self;
  self=(GedWindow *)data;
  /*we're not modal, but don't put two dialogs up.*/
  if(self->close_dialog)
    return 1;
  /*see if they actually have any unsaved changes, because if they do,
    we need to ask them.*/
  buffer=gtk_text_view_get_buffer(GTK_TEXT_VIEW(self->text_view));
  modified=gtk_text_buffer_get_modified(buffer);
  if(!modified)
    {
      do_quit(self);
      return 0;
    }
  /*we ask them about their unsaved changes with a dialog box*/
  self->close_dialog=gtk_dialog_new_with_buttons("Save changes to file before"
						 " closing?",
						 GTK_WINDOW(window),0,
						 "Close without saving",0,
						 GTK_STOCK_CANCEL,1,
						 GTK_STOCK_SAVE,2,
						 NULL);
  g_signal_connect(self->close_dialog,"response",
		   G_CALLBACK(close_dialog_answered),
		   data); /*data is void* form of self*/
  gtk_widget_show(self->close_dialog);
  return 1;
}

void close_dialog_answered(GtkDialog *dialog,gint id,gpointer data)
{
  GedWindow *self;
  self=(GedWindow *)data;

  gtk_widget_destroy(self->close_dialog);
  self->closing=1;
  switch(id)
    {
    case 0:
      do_quit(self);
      break;
    case 2:
      save_item_clicked(NULL,data); /*data is void* form of self*/
      /*do_quit(); handled in save_file*/
      break;
    case 1:
    default:
      self->closing=0;
      self->close_dialog=0;
      break;
    }
}

void new_item_clicked(GtkMenuItem *item,gpointer data)
{
  GtkTextBuffer *buffer;
  GedWindow *self;
  self=(GedWindow *)data;
  buffer=gtk_text_view_get_buffer(GTK_TEXT_VIEW(self->text_view));
  draw_ged_window();
}

void open_item_clicked(GtkMenuItem *item,gpointer data)
{
  GedWindow *self;
  self=(GedWindow *)data;
  self->file_selector=gtk_file_selection_new("Select file to open...");
  g_signal_connect(GTK_FILE_SELECTION(self->file_selector)->ok_button,
		   "clicked",G_CALLBACK(open_selected_file),data);
  g_signal_connect_swapped(GTK_FILE_SELECTION(self->file_selector)->
			   cancel_button,"clicked",
			   G_CALLBACK(gtk_widget_destroy),self->file_selector);
  gtk_widget_show(self->file_selector);
}

void open_selected_file(GtkButton *button,gpointer data)
{
  const gchar *nm;
  GedWindow *self;
  self=(GedWindow *)data;
  nm=gtk_file_selection_get_filename(GTK_FILE_SELECTION(self->file_selector));
  open_file(self,nm);
  gtk_widget_destroy(self->file_selector);
  self->file_selector=0;
}

void save_item_clicked(GtkMenuItem *item,gpointer data)
{
  GedWindow *self;
  self=(GedWindow *)data;
  if(!self->current_filename)
    save_as_item_clicked(NULL,data);
  else
    save_file(self,self->current_filename);
}

void save_as_item_clicked(GtkMenuItem *item,gpointer data)
{
  GedWindow *self;
  self=(GedWindow *)data;
  self->file_selector=gtk_file_selection_new("Select location to save...");
  g_signal_connect(GTK_FILE_SELECTION(self->file_selector)->ok_button,
		   "clicked",G_CALLBACK(save_selected_file),data);
  g_signal_connect_swapped(GTK_FILE_SELECTION(self->file_selector)->
			   cancel_button,"clicked",
			   G_CALLBACK(gtk_widget_destroy),self->file_selector);
  gtk_widget_show(self->file_selector);
}

void save_selected_file(GtkButton *button,gpointer data)
{
  const gchar *nm;
  GedWindow *self;
  self=(GedWindow *)data;
  nm=gtk_file_selection_get_filename(GTK_FILE_SELECTION(self->file_selector));
  save_file(self,nm);
  gtk_widget_destroy(self->file_selector);
  self->file_selector=0;
}

void quit_item_clicked(GtkMenuItem *item,gpointer data)
{
  GedWindow *self;
  self=(GedWindow *)data;
  user_quit(self->window,NULL,data);
}

void cut_item_clicked(GtkMenuItem *item,gpointer data)
{
  GtkClipboard *clip;
  GtkTextBuffer *buffer;
  gboolean editable;
  GedWindow *self;
  self=(GedWindow *)data;
  clip=gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
  buffer=gtk_text_view_get_buffer(GTK_TEXT_VIEW(self->text_view));
  editable=gtk_text_view_get_editable(GTK_TEXT_VIEW(self->text_view));
  gtk_text_buffer_cut_clipboard(buffer,clip,editable);
}

void copy_item_clicked(GtkMenuItem *item,gpointer data)
{
  GtkClipboard *clip;
  GtkTextBuffer *buffer;
  gboolean editable;
  GedWindow *self;
  self=(GedWindow *)data;
  clip=gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
  buffer=gtk_text_view_get_buffer(GTK_TEXT_VIEW(self->text_view));
  gtk_text_buffer_copy_clipboard(buffer,clip);
}

void paste_item_clicked(GtkMenuItem *item,gpointer data)
{
  GtkClipboard *clip;
  GtkTextBuffer *buffer;
  gboolean editable;
  GedWindow *self;
  self=(GedWindow *)data;
  clip=gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
  buffer=gtk_text_view_get_buffer(GTK_TEXT_VIEW(self->text_view));
  editable=gtk_text_view_get_editable(GTK_TEXT_VIEW(self->text_view));
  gtk_text_buffer_paste_clipboard(buffer,clip,0,editable);
}

void word_wrap_item_clicked(GtkMenuItem *item,gpointer data)
{
  fprintf(stderr,"Not yet implemented\n");
}
