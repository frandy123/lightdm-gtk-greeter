#include <gtk/gtk.h>
#include "greetermenubar.h"


static void greeter_menu_bar_size_allocate(GtkWidget* widget, GtkAllocation* allocation);

G_DEFINE_TYPE(GreeterMenuBar, greeter_menu_bar, GTK_TYPE_MENU_BAR);

static void
greeter_menu_bar_class_init(GreeterMenuBarClass* klass)
{
	GtkWidgetClass* widget_class = GTK_WIDGET_CLASS(klass);
	widget_class->size_allocate = greeter_menu_bar_size_allocate;
}

static void
greeter_menu_bar_init(GreeterMenuBar* square)
{

}

GtkWidget* 
greeter_menu_bar_new()
{
	return GTK_WIDGET(g_object_new(greeter_menu_bar_get_type(), NULL));
}

#if GTK_CHECK_VERSION (3, 0, 0)
static gint
sort_minimal_size(gconstpointer a, gconstpointer b, GtkRequestedSize* sizes)
{
    gint a_size = sizes[GPOINTER_TO_INT(a)].natural_size;
    gint b_size = sizes[GPOINTER_TO_INT(b)].natural_size;
    return a_size == b_size ? 0 : a_size > b_size ? -1 : +1; 
}

static void
greeter_menu_bar_size_allocate(GtkWidget* widget, GtkAllocation* allocation)
{
    GList* item;
    GList* shell_children;
    GList* expand_nums = NULL;
    guint visible_count = 0, expand_count = 0;

	g_return_if_fail(allocation != NULL);
	g_return_if_fail(GREETER_IS_MENU_BAR(widget));

    gtk_widget_set_allocation(widget, allocation);

    GtkPackDirection pack_direction = gtk_menu_bar_get_pack_direction(GTK_MENU_BAR(widget));
    g_return_if_fail(pack_direction == GTK_PACK_DIRECTION_LTR || pack_direction == GTK_PACK_DIRECTION_RTL);

    shell_children = gtk_container_get_children(GTK_CONTAINER(widget));

    for(item = shell_children; item; item = g_list_next(item))
        if(gtk_widget_get_visible(item->data))
        {
            if(gtk_widget_compute_expand(item->data, GTK_ORIENTATION_HORIZONTAL))
            {
                expand_nums = g_list_prepend(expand_nums, GINT_TO_POINTER(visible_count));
                expand_count++;
            }
            visible_count++;
        }

    if(gtk_widget_get_realized(widget))
        gdk_window_move_resize(gtk_widget_get_window(widget),
                               allocation->x, allocation->y,
                               allocation->width, allocation->height);

    if(visible_count > 0)
    {
        GtkAllocation remaining_space;
        GtkStyleContext* context = gtk_widget_get_style_context(widget);
        GtkStateFlags flags = gtk_widget_get_state_flags(widget);
        GtkRequestedSize* requested_sizes = g_newa(GtkRequestedSize, visible_count);
        guint border_width = gtk_container_get_border_width(GTK_CONTAINER(widget));
        GtkShadowType shadow_type = GTK_SHADOW_OUT;
        GtkBorder border;
        gint toggle_size;

        gtk_style_context_get_padding(context, flags, &border);
        gtk_widget_style_get(widget, "shadow-type", &shadow_type, NULL);

        remaining_space.x = (border_width + border.left);
        remaining_space.y = (border_width + border.top);
        remaining_space.width = allocation->width -
                                2 * border_width - border.left - border.right;
        remaining_space.height = allocation->height -
                                 2 * border_width - border.top - border.bottom;

        if (shadow_type != GTK_SHADOW_NONE)
        {
            gtk_style_context_get_border(context, flags, &border);

            remaining_space.x += border.left;
            remaining_space.y += border.top;
            remaining_space.width -= border.left + border.right;
            remaining_space.height -= border.top + border.bottom;
        }

        GtkRequestedSize* request = requested_sizes;
        int size = remaining_space.width;
        gboolean ltr = (gtk_widget_get_direction(widget) == GTK_TEXT_DIR_LTR) == (pack_direction == GTK_PACK_DIRECTION_LTR);

        for(item = shell_children; item; item = g_list_next(item))
        {
            if (!gtk_widget_get_visible(item->data))
                continue;

            request->data = item->data;
            gtk_widget_get_preferred_width_for_height(item->data, remaining_space.height,
                    &request->minimum_size,
                    &request->natural_size);
            gtk_menu_item_toggle_size_request(GTK_MENU_ITEM(item->data),
                                              &toggle_size);
            request->minimum_size += toggle_size;
            request->natural_size += toggle_size;

            gtk_menu_item_toggle_size_allocate(GTK_MENU_ITEM(item->data), toggle_size);

            size -= request->minimum_size;
            request++;
        }

        size = gtk_distribute_natural_allocation(size, visible_count, requested_sizes);

        /* Distribution extra space for widgets with expand=True */
        if(size > 0 && expand_nums)
        {
            expand_nums = g_list_sort_with_data(expand_nums, (GCompareDataFunc)sort_minimal_size,
                                                requested_sizes);
            GList* first_item = expand_nums;
            gint needed_size = -1;
            gint max_size = requested_sizes[GPOINTER_TO_INT(first_item->data)].natural_size;
            gint total_needed_size = 0;


            /* Free space that all widgets need to have the same (max_size) width
             * [___max_width___][widget         ][widget____     ]
             * total_needed_size := [] + [         ] + [     ]
             * total_needed_size = [              ]
             */
            for(item = g_list_next(expand_nums); item; item = g_list_next(item))
                total_needed_size += max_size - requested_sizes[GPOINTER_TO_INT(item->data)].natural_size;

            while(first_item)
            {
                if(size >= total_needed_size)
                {
                    /* total_needed_size is enough for all remaining widgets */
                    needed_size = max_size + (size - total_needed_size)/expand_count;
                    break;
                }
                /* Removing current maximal widget from list */
                total_needed_size -= max_size - requested_sizes[GPOINTER_TO_INT(item->data)].natural_size;
                first_item = g_list_next(first_item);
                if(first_item)
                    max_size = requested_sizes[GPOINTER_TO_INT(first_item->data)].natural_size;
            }

            for(item = first_item; item; item = g_list_next(item))
            {
                request = &requested_sizes[GPOINTER_TO_INT(item->data)];
                gint dsize = needed_size - request->natural_size;
                if(size < dsize)
                    dsize = size;
                size -= dsize;
                request->natural_size += dsize;
            }
        }
        
        gint i;
        for(i = 0; i < visible_count; i++)
        {
            GtkAllocation child_allocation = remaining_space;
            request = &requested_sizes[i];

            child_allocation.width = request->natural_size;
            remaining_space.width -= request->natural_size;
            if (ltr)
                remaining_space.x += request->natural_size;
            else
                child_allocation.x += remaining_space.width;
            gtk_widget_size_allocate(request->data, &child_allocation);
        }
        g_list_free(expand_nums);
    }
    g_list_free(shell_children);
}
#else
static gboolean
widget_get_expand (GtkWidget* widget)
{
    return g_object_get_data(G_OBJECT(widget),
                             GREETER_MENU_BAR_EXPAND_PROP) != NULL;
}

struct RequestedSize
{
    GtkWidget* child;
    GtkRequisition size;
};

static gint
sort_minimal_size(gconstpointer a, gconstpointer b, struct RequestedSize* sizes)
{
    gint a_size = sizes[GPOINTER_TO_INT(a)].size.width;
    gint b_size = sizes[GPOINTER_TO_INT(b)].size.width;
    return a_size == b_size ? 0 : a_size > b_size ? -1 : +1; 
}

static void
greeter_menu_bar_size_allocate(GtkWidget* widget, GtkAllocation* allocation)
{
    GList* item;
    GList* shell_children;
    GList* expand_nums = NULL;
    guint visible_count = 0, expand_count = 0;

    g_return_if_fail (GTK_IS_MENU_BAR (widget));
    g_return_if_fail (allocation != NULL);

    GtkPackDirection pack_direction = gtk_menu_bar_get_pack_direction(GTK_MENU_BAR(widget));
    g_return_if_fail(pack_direction == GTK_PACK_DIRECTION_LTR || pack_direction == GTK_PACK_DIRECTION_RTL);
 
    shell_children = GTK_MENU_SHELL (widget)->children;

    for(item = shell_children; item; item = g_list_next(item))
        if(gtk_widget_get_visible(item->data))
        {
            if(widget_get_expand(item->data))
            {
                expand_nums = g_list_prepend(expand_nums, GINT_TO_POINTER(visible_count));
                expand_count++;
            }
            visible_count++;
        }

    widget->allocation = *allocation;
    if (gtk_widget_get_realized (widget))
        gdk_window_move_resize (widget->window,
                                allocation->x, allocation->y,
                                allocation->width, allocation->height);

    if (visible_count)
    {
        gint toggle_size, ipadding;
        GtkTextDirection direction = gtk_widget_get_direction (widget);
        GtkShadowType shadow_type = GTK_SHADOW_OUT;
        GtkAllocation remaining_space;

        gtk_widget_style_get (widget, "shadow-type", &shadow_type, NULL);
        gtk_widget_style_get (widget, "internal-padding", &ipadding, NULL);

        remaining_space.x = (GTK_CONTAINER (widget)->border_width + ipadding);
        remaining_space.y = (GTK_CONTAINER (widget)->border_width);
        remaining_space.width = allocation->width - 2 * remaining_space.x;
        remaining_space.height = allocation->height - 2 * remaining_space.y;

        if (shadow_type != GTK_SHADOW_NONE)
        {
            remaining_space.x += widget->style->xthickness;
            remaining_space.y += widget->style->ythickness;
            remaining_space.width -= 2 * widget->style->xthickness;
            remaining_space.height -= 2 * widget->style->ythickness;
        }

        struct RequestedSize* requested_sizes = g_newa(struct RequestedSize, visible_count);
        struct RequestedSize* request = requested_sizes;
        int size = remaining_space.width;

        for(item = shell_children; item; item = g_list_next(item))
        {
            if (!gtk_widget_get_visible (item->data))
                continue;
            request->child = item->data;
            gtk_menu_item_toggle_size_request (GTK_MENU_ITEM (request->child), &toggle_size);
            gtk_menu_item_toggle_size_allocate (GTK_MENU_ITEM (request->child), toggle_size);
            gtk_widget_get_child_requisition (request->child, &request->size);
            request->size.width += toggle_size;
            request->size.height += toggle_size;
            size -= request->size.width;
            ++request;
        }

        if(size > 0 && expand_nums)
        {
            expand_nums = g_list_sort_with_data(expand_nums, (GCompareDataFunc)sort_minimal_size,
                                                requested_sizes);
            GList* first_item = expand_nums;
            gint needed_size = -1;
            gint max_size = requested_sizes[GPOINTER_TO_INT(first_item->data)].size.width;
            gint total_needed_size = 0;

            for(item = g_list_next(expand_nums); item; item = g_list_next(item))
                total_needed_size += max_size - requested_sizes[GPOINTER_TO_INT(item->data)].size.width;

            while(first_item)
            {
                if(size >= total_needed_size)
                {
                    needed_size = max_size + (size - total_needed_size)/expand_count;
                    break;
                }
                total_needed_size -= max_size - requested_sizes[GPOINTER_TO_INT(item->data)].size.width;
                first_item = g_list_next(first_item);
                if(first_item)
                    max_size = requested_sizes[GPOINTER_TO_INT(first_item->data)].size.width;
            }

            for(item = first_item; item; item = g_list_next(item))
            {
                request = &requested_sizes[GPOINTER_TO_INT(item->data)];
                gint dsize = needed_size - request->size.width;
                if(size < dsize)
                    dsize = size;
                size -= dsize;
                request->size.width += dsize;
            }
        }

        gboolean ltr = (direction == GTK_TEXT_DIR_LTR) == (pack_direction == GTK_PACK_DIRECTION_LTR);
        GtkAllocation child_allocation;
        gint i;
        for(i = 0; i < visible_count; i++)
        {
            child_allocation = remaining_space;
            request = &requested_sizes[i];

            child_allocation.width = request->size.width;
            remaining_space.width -= request->size.width;
            if (ltr)
                remaining_space.x += request->size.width;
            else
                child_allocation.x += remaining_space.width;
            gtk_widget_size_allocate(request->child, &child_allocation);
        }
        g_list_free(expand_nums);
    }
}
#endif
