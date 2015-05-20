#include <stdint.h>
#include <stdlib.h>
#include <errno.h>
#include <netinet/in.h>
#include <gtk/gtk.h>

int
socket_create(void) {
  int fd = socket(AF_INET, SOCK_STREAM, 0);
  if (fd == -1) perror("cannot create socket");
  return fd;
}

struct sockaddr_in
socket_build_addr(uint16_t port) {
  struct sockaddr_in addr = {0};
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = inet_addr("172.20.50.18");
  addr.sin_port = htons(port);
  return addr;
}

void
socket_wait(void) {
  time_t sec = 0;
  struct timespec time = {sec, 233333333}, rem;
  nanosleep(&time, &rem);
}

void
socket_send(int fd, uint8_t * data) {
  //uint8_t i = 0, len = 12;
  //for (; i < len; i++) {
  //  printf(" %02X", data[i]);
  //}
  //puts(" >");
  ssize_t ret = send(fd, data, 12, 0);
  if (ret == -1 || ret != 12) perror("cannot send socket to server");
}

ssize_t
socket_pull(int fd, void * buf, size_t len) {
  ssize_t ret = -1;
  ssize_t i = 0;
  for (; i < len; ) {
    ret = recv(fd, buf, len - i, MSG_DONTWAIT);
    printf("ret %zd len %zu", ret, len);
    int j = 0;
    for (; j < ret; j++) {
      printf(" %02X", ((uint8_t *) buf)[j]);
    }
    putchar('\n');
    fflush(stdout);
    if (ret <= 0) {
      return ret;
    } else {
      i += ret;
    }
  }
  return ret;
}

void
socket_recv(int fd, void * buf, size_t len) {
  ssize_t ret = socket_pull(fd, buf, len);
  if (ret == 0)
    perror("server disconnected");
  else if (ret == -1 && errno != EAGAIN && errno != EWOULDBLOCK)
    perror("cannot recv from server");
}

uint8_t
socket_read(int fd) {
  uint8_t buf[39] = {0};
  socket_recv(fd, buf, 9);
  if (buf[7] < 0) perror("recv error");
  socket_recv(fd, buf + 9, buf[8]);
  return buf[10];
}

void
socket_connect(uint8_t * data, uint8_t * tmp, uint8_t * power) {
  int fd = socket_create();
  struct sockaddr_in addr = socket_build_addr(502);
  int ret = connect(fd, (struct sockaddr *) &addr, sizeof(struct sockaddr));
  if (ret == -1) perror("cannot connect server");
  printf("connected\n");
  fflush(stdout);
  data[data[0] * 0xC + 0xA] = 16;
  socket_send(fd, data + data[0] * 12 + 1);
  socket_wait();
  * power = socket_read(fd);
  data[10 + data[0] * 12] += 0xA;
  socket_send(fd, data + data[0] * 12 + 1);
  socket_wait();
  * tmp = socket_read(fd);
  close(fd);
}

void *
gdk_draw_text(void * data) {
  uint8_t tmp, power;
  socket_connect(data, &tmp, &power);
  return (void *) (uintptr_t) (tmp << 1 | !power);
}

gboolean
gtk_drawing_area_update(GtkWidget * area, cairo_t * cr, gpointer data) {
  pthread_t thread;
  int ret = pthread_create(&thread, 0, gdk_draw_text, data);
  if (ret == -1) perror("could not create thread");
  void * result;
  pthread_join(thread, &result);
  uint8_t tmp = (uintptr_t) result >> 2;
  char num[2] = {0};
  cairo_select_font_face(cr, "Droid Sans Fallback",
                         CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
  if (tmp) {
    cairo_set_font_size(cr, 72);
    cairo_set_source_rgba(cr, 0, 0, 0, 1);
    cairo_move_to(cr, 120, 100);
    num[0] = '0' + tmp % 100 / 10;
    cairo_show_text(cr, num);
    num[0] = '0' + tmp % 10;
    cairo_show_text(cr, num);
    cairo_show_text(cr, " °C");
    cairo_set_font_size(cr, 24);
    cairo_move_to(cr, 140, 140);
    cairo_show_text(cr, "power is ");
    if ((uintptr_t) result % 2) {
      cairo_set_source_rgba(cr, 0.6, 0, 0, 1);
      cairo_show_text(cr, "off");
    } else {
      cairo_set_source_rgba(cr, 0, 0.6, 0, 1);
      cairo_show_text(cr, "on");
    }
  } else {
    cairo_set_font_size(cr, 16);
    cairo_move_to(cr, 50, 110);
    cairo_show_text(cr, "晚上冷氣已自動關閉，請於上午或下午再試");
  }
  return FALSE;
}

void
gtk_room_menu_item_pressed(GtkMenuItem * menu_item, gpointer data) {
  ** (uint8_t **) data = * ((uint8_t *) data + sizeof(uintptr_t));
  GtkWidget * draw = * (GtkWidget **) (* (uint8_t **) data + 25);
  gtk_widget_queue_draw(draw);
}

GtkWidget *
gtk_room_menu_item_new(uint8_t * data) {
  GtkWidget * room_item_menu = gtk_menu_item_new_with_mnemonic("教室");
  GtkWidget * room_menu = gtk_menu_new();

  char * names[] = {"A209", "A211"};
  static uint8_t opt[2 * sizeof(uintptr_t) + 2];
  int i, len = sizeof(names) / sizeof(names[0]);
  for (i = 0; i < len; i++) {
    GtkWidget * name_item_menu = gtk_menu_item_new_with_mnemonic(names[i]);
    * (void **) (opt + i + i * sizeof(uintptr_t)) = data;
    * (opt + i + sizeof(uintptr_t) + i * sizeof(uintptr_t)) = i;
    g_signal_connect(G_OBJECT(name_item_menu), "activate",
                     G_CALLBACK(gtk_room_menu_item_pressed),
                     opt + i + i * sizeof(uintptr_t));
    gtk_menu_shell_append(GTK_MENU_SHELL(room_menu), name_item_menu);
  }

  gtk_menu_item_set_submenu(GTK_MENU_ITEM(room_item_menu), room_menu);
  return room_item_menu;
}

int
main(int argc, char * argv[]) {
  gtk_init(&argc, &argv);

  GtkWidget * window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title(GTK_WINDOW(window), "冷氣讀取器");
  gtk_window_set_default_size(GTK_WINDOW(window), 400, 300);
  g_signal_connect(G_OBJECT(window), "destroy",
                   G_CALLBACK(gtk_main_quit), NULL);

  GtkWidget * draw = gtk_drawing_area_new();
  GdkRGBA color = {1, 1, 1, 1};
  uint8_t data[sizeof(uintptr_t) + 0x19] =
  {0, 0, 0, 0, 0, 0, 6, 8, 3, 1, 16, 0, 1, 0, 0, 0, 0, 0, 6,
    10, 3, 1, 16, 0, 1};
  * (void **) &data[25] = draw;
  g_signal_connect(G_OBJECT(draw), "draw",
                   G_CALLBACK(gtk_drawing_area_update), data);

  GtkWidget * menubar = gtk_menu_bar_new();
  gtk_menu_shell_append(GTK_MENU_SHELL(menubar),
                        gtk_room_menu_item_new(data));
  gtk_widget_set_margin_left(menubar, 10);

  GtkWidget * vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
  gtk_box_pack_start(GTK_BOX(vbox), menubar, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), draw, TRUE, TRUE, 0);
  gtk_container_add(GTK_CONTAINER(window), vbox);

  GtkCssProvider * css = gtk_css_provider_new();
  gtk_css_provider_load_from_path(css, "main.css", NULL);
  gtk_style_context_add_provider_for_screen(
    gdk_screen_get_default(), GTK_STYLE_PROVIDER(css),
    GTK_STYLE_PROVIDER_PRIORITY_USER);

  gtk_widget_show_all(window);
  gtk_main();
  return 0;
}
