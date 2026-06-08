//#include <cstdint>
#include <math.h>
#include <stdio.h>
#include <fcntl.h>

#include <stdlib.h>
#include <unistd.h>    // close(), read()
#include <string.h>    // strerror()

#include <libevdev-1.0/libevdev/libevdev.h>
#include <libevdev-1.0/libevdev/libevdev-uinput.h>

#include <gtk/gtk.h>

int activated = 0 ;

char* SelectedDev ;

guint timeout_id = 0 ;

int mainInterval = 10;

int min(int a , int b){
    if (a > b ){
        return b ;
    }
    return a;
}

int max(int a , int b){
    if (a > b ){
        return a ;
    }
    return b;
}

void readRC(int rc, struct libevdev *dev){
    struct input_event ev;
    //struct libevdev *dev = NULL;

    while (1) {
        int rc = libevdev_next_event(dev,
                                    LIBEVDEV_READ_FLAG_NORMAL | LIBEVDEV_READ_FLAG_BLOCKING,
                                    &ev);

        if (rc == LIBEVDEV_READ_STATUS_SUCCESS) {
            // Ignorer les events de sync (EV_SYN), ils sont juste des séparateurs
            if (ev.type == EV_ABS) {
                printf("Axe: code=%d  valeur=%d\n", ev.code, ev.value);
            }
            if (ev.type == EV_KEY) {
                printf("Bouton: code=%d  %s\n", ev.code,
                    ev.value ? "appuyé" : "relâché");
            }
        }
    }
}


struct input_event RumbleEffect(struct libevdev *dev){
    //struct libevdev *dev = NULL;
    
    //FF_RUMBLE Vibration simple"clac" quand tu enclenchs
    //FF_CONSTANT Force constante dans une directionRésistance sur un axe
    //FF_SPRING Rappel vers le centreLevier qui revient en neutre

    int fd = libevdev_get_fd(dev);
    struct ff_effect effect;
    memset(&effect, 0, sizeof(effect));

    effect.type = FF_RUMBLE;
    effect.id   = -1;  // -1 = demande au driver d'assigner un id libre

    effect.u.rumble.strong_magnitude = 0xffff; // intensité moteur fort (0 à 0xffff)
    effect.u.rumble.weak_magnitude   = 0x0000; // intensité moteur faible

    effect.replay.length = 150/2;  // durée en ms
    effect.replay.delay  = 0;    // délai avant déclenchement
    struct input_event play;
    if (ioctl(fd, EVIOCSFF, &effect) < 0) {
        perror("upload FF");
        return play;
    }

    
    memset(&play, 0, sizeof(play));
    play.type  = EV_FF;
    play.code  = effect.id;  // l'id assigné par le driver
    play.value = 1;          // 1 = jouer, 0 = stopper

    
    return play;
}

static int ff_initialized = 0;
struct ff_effect move;
struct ff_effect move2;

struct libevdev_uinput *uidev = NULL;

void ChangeSettingMove(int fd){
    if (!ff_initialized) return;

    if (ioctl(fd, EVIOCSFF, &move) < 0) {
        perror("init FF constant");
        return;
    }
}

void ConstMove(struct libevdev *dev){
    //struct libevdev *dev = NULL;
    
    //FF_RUMBLE Vibration simple"clac" quand tu enclenchs
    //FF_CONSTANT Force constante dans une directionRésistance sur un axe
    //FF_SPRING Rappel vers le centreLevier qui revient en neutre

    int fd = libevdev_get_fd(dev);
    
    memset(&move, 0, sizeof(move));
    
    move.type = FF_CONSTANT;
    move.id = -1;  // -1 = demande au driver d'assigner un id libre
    move.replay.length        = 0xFFFF;   // durée "infinie"
    move.replay.delay         = 0;
    move.u.constant.level     = 0x7FFF;
    move.direction            = 0xC000;

    move.u.constant.envelope.attack_length  = 0;
    move.u.constant.envelope.attack_level   = 0;
    move.u.constant.envelope.fade_length    = 50;  // 50ms de relâche
    move.u.constant.envelope.fade_level     = 0;

    struct input_event play;

    if (ioctl(fd, EVIOCSFF, &move) < 0) {
        perror("init FF constant");
        return;
    }

    // Démarrer l'effet (il reste actif, on met juste à jour son level)
    memset(&play, 0, sizeof(play));
    play.type  = EV_FF;
    play.code  = move.id;
    play.value = 1;
    write(fd, &play, sizeof(play));

    ff_initialized = 1;
}



void XCenterForce(int fd,int Xpos,int DeltaX){
    if (Xpos>0){
        move.direction = 0x4000;
        ChangeSettingMove(fd);
    }
    else {
        //printf("Xpos L : %d\n",Xpos);
        move.direction = 0xC000;
        ChangeSettingMove(fd);
    }
    
    //int force = 10000 ;
    
    int force = abs(Xpos)*(63.998046875*0.8) - (abs(DeltaX)*1000);
    force = min(max(force,0), 32767);

    if (force<0){force = 0;}
    move.u.constant.level = force;
    ChangeSettingMove(fd);
}

void readRControl(int rc, struct libevdev *dev){
    struct input_event ev;
    struct input_event rumble = RumbleEffect(dev);
    int fd = libevdev_get_fd(dev);
    ConstMove(dev);
    write(fd, &move, sizeof(move));

    int Xaxe = 0 ;
    int Yaxe = 1 ;

    int Xpos = 0 ;
    int Ypos = 0 ;

    int LXpos = 0 ;
    int LYpos = 0 ;

    int RXpos = 0 ;
    int RYpos = 1 ;

    int Size = 1024;
    int borderSize = 50;
    int SizeSpeed = (1024/3)-borderSize;
    
    int Speed = -1;
    int LSpeed = -1;

    int DeltaX = 0;
    int DeltaY = 0;
    //wait until ready
    while (!ff_initialized){};

    

    while (1) {
        int rc = libevdev_next_event(dev,
                                    LIBEVDEV_READ_FLAG_NORMAL | LIBEVDEV_READ_FLAG_BLOCKING,
                                    &ev);

        if (rc == LIBEVDEV_READ_STATUS_SUCCESS) {
            if (ev.type == EV_ABS) {

                DeltaX = Xpos - LXpos;
                DeltaY = Ypos - LYpos;

                if (ev.code == Xaxe){
                    Xpos = ev.value;
                    
                }
                else if (ev.code == Yaxe){
                    Ypos = ev.value;
                }

                



                if (abs(Ypos) > 350 ){
                     move.u.constant.level = 32767/2;
                    if ((Ypos)>0){
                        move.direction = 0x0000;
                        ChangeSettingMove(fd);
                    }
                    else {
                        move.direction = 0x8000;
                        ChangeSettingMove(fd);
                    }



                    RXpos = Xpos+512;RYpos = Ypos+512;
                    if (Speed == -1){
                        if (Ypos>0){
                            Speed = floor(RXpos/(1024/3));
                        }
                        else{
                            Speed = floor(RXpos/(1024/3)+3);
                        }
                    }
                
                    
                }
                else {



                    Speed = -1;
                    //int force = 10000 ;

                    

                    XCenterForce(fd,Xpos,DeltaX);
                    //YCenterForce(fd,Ypos,DeltaY);
                }





                if (Speed!=LSpeed){
                        write(fd, &rumble, sizeof(rumble));
                        printf("speed : %d %d\n",Speed,LSpeed);
                        libevdev_uinput_write_event(uidev, EV_KEY, BTN_TRIGGER_HAPPY2 +LSpeed, 0);
                        libevdev_uinput_write_event(uidev, EV_SYN, SYN_REPORT, 0);
                        
                        libevdev_uinput_write_event(uidev, EV_KEY, BTN_TRIGGER_HAPPY2 +Speed, 1); // appui
                        libevdev_uinput_write_event(uidev, EV_SYN, SYN_REPORT, 0);
                }

                LXpos = Xpos;
                LYpos = Ypos;
                LSpeed = Speed;
                //printf("x=%d  y=%d\n", Xpos, Ypos);
            }
        }
    }
}



int getfdFromPath(const char* path){
    int fd = open(path, O_RDWR | O_NONBLOCK);
    if (fd < 0) {
        perror("open");
        return -1;
    }
    return fd;
}

int getrcFromfd(const int fd , struct libevdev **dev){
    int rc = libevdev_new_from_fd(fd, dev);
    if (rc < 0) {
        fprintf(stderr, "Erreur libevdev: %s\n", strerror(-rc));
        return -1;
    }
    return rc;
}

char** GetDevicesNames(char* names[],int *n){
    struct libevdev *dev = NULL;
    char* path = malloc(250);
    int peripheral_Number = 0;
    
    while (1){
        sprintf(path, "/dev/input/event%d", peripheral_Number);
        if (access(path, F_OK) == 0) {
            int fd = getfdFromPath(path);
            if (fd == -1){free(path);printf("fd -1\n");return 0;}

            int rc = getrcFromfd(fd,&dev);
            if (rc == -1){free(path);printf("rc -1\n");return 0;}
            names[peripheral_Number] = libevdev_get_name(dev);
            peripheral_Number++;
        }
        else{break;}
    }
    *n = peripheral_Number;
    return names;
}

int FindDeviceByName(char* name, struct libevdev **dev){

    char* path = malloc(250);

    int perNum = 0;
    while (1)
    {
        
        //create path for the peripheral
        sprintf(path, "/dev/input/event%d", perNum);
        printf("%s : ",path);
        if (access(path, F_OK) == 0) {
            // if file exists
            int fd = getfdFromPath(path);
            if (fd == -1){free(path);printf("fd -1\n");return -1;}

            int rc = getrcFromfd(fd,dev);
            if (rc == -1){free(path);printf("rc -1\n");return -1;}

            printf("%s\n",libevdev_get_name(*dev));

            if (strcmp(libevdev_get_name(*dev),name) == 0){
                printf("joystick found\n");
                close(fd);
                free(path);
                return rc;
            }
            // if the peripheral is not the one we search, we remove it and set the counter to ++ to setup the next one
            libevdev_free(*dev);
            //*dev = NULL;
            close(fd);
            perNum++;

        } else {
            // if file doesn't exist
            printf("file dont exist\n");
            break;
        }
        


    }
    free(path);
    return -1;
}

void ChangePeripheral(GtkWidget *combo){
    printf("\n%s\n",gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(combo)));
}

void RefreshPeripheral(GtkWidget *button,gpointer data){
    GtkWidget *combo = GTK_WIDGET(data);
    printf("Refresh\n");
    int devNumber;
    char* names[255];
    GetDevicesNames(names,&devNumber);
    gtk_combo_box_text_remove_all(GTK_COMBO_BOX_TEXT(combo));
    

    for(int i = 0 ; i < devNumber;i++){
        printf("%d:%s\n",i,names[i]);
        gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo), names[i]);
    }
    gtk_combo_box_set_active(GTK_COMBO_BOX(combo),0);
}

gboolean placholder(gpointer data){
    printf("placeholder\n");
    return G_SOURCE_CONTINUE;
}

void activate(GtkWidget *button,gpointer data){

    GtkWidget *window = GTK_WIDGET(data);

    printf("activate : %d\n",activated);

    if (activated){
        gtk_window_set_title(GTK_WINDOW(window), "JoytoGear - deactivated");
        g_source_remove(timeout_id);
        timeout_id = 0;
        activated=0;
    }
    else{

        gtk_window_set_title(GTK_WINDOW(window), "JoytoGear - activated");

        timeout_id = g_timeout_add(mainInterval,  placholder, NULL);
        activated=1;
    }
    
    
}

GtkWidget* init_gtk(int argc, char *argv[]){

    gtk_init(&argc, &argv);
    GtkWidget *boxLeft = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    GtkWidget *boxRight = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);

    gtk_window_set_title(GTK_WINDOW(window), "JoytoGear - deactivated");
    gtk_window_set_default_size(GTK_WINDOW(window), 400, 300);

    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);
    
    

    GtkWidget *combo = gtk_combo_box_text_new();
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo), "Aucun");
    gtk_combo_box_set_active(GTK_COMBO_BOX(combo),0);


    GtkWidget *btnActivate = gtk_toggle_button_new_with_label("Activer");
    //gboolean actif = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(btnActivate));

    GtkWidget *Refresh = gtk_button_new_with_label("Refresh Devices");
    //---------------------------------
    //left side
    gtk_box_pack_start(GTK_BOX(boxLeft), Refresh, TRUE, TRUE, 0);
    gtk_box_pack_end(GTK_BOX(boxLeft), combo, TRUE, TRUE, 0);

    //---------------------------------
    //right side
    gtk_box_pack_start(GTK_BOX(boxRight), btnActivate, TRUE, TRUE, 0);

    GtkWidget *boxWindow = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_box_pack_start(GTK_BOX(boxWindow), boxLeft, TRUE, TRUE, 0);
    gtk_box_pack_end(GTK_BOX(boxWindow), boxRight, TRUE, TRUE, 0);

    //if the user change dev , call the ChangePeripheral function
    g_signal_connect(combo, "changed", G_CALLBACK(ChangePeripheral), NULL);
    //refresh dev button
    g_signal_connect(Refresh, "clicked", G_CALLBACK(RefreshPeripheral), combo);
    //
    g_signal_connect(btnActivate, "toggled", G_CALLBACK(activate), window);

    gtk_container_add(GTK_CONTAINER(window), boxWindow);
    return window;
}



int main(int argc, char *argv[]) {

    GtkWidget *window = init_gtk(argc,argv);

    struct libevdev *dev = NULL;



    int rc = FindDeviceByName("Microsoft SideWinder Force Feedback 2 Joystick",&dev);
    if (rc < 0){return -1;}

    struct libevdev *vdev = libevdev_new();
    libevdev_set_name(vdev, "virtual-shifter");

    libevdev_enable_event_type(vdev, EV_KEY);
    for (int i = 0; i < 8; i++) {
        libevdev_enable_event_code(vdev, EV_KEY, BTN_TRIGGER_HAPPY1 + i, NULL);
    }

    
    int rc2 = libevdev_uinput_create_from_device(vdev,LIBEVDEV_UINPUT_OPEN_MANAGED,&uidev);
    if (rc2 < 0) {
    //    fprintf(stderr, "Erreur uinput: %s\n", strerror(-rc));
        return 1;
    }
    libevdev_free(vdev);
    //readRControl(rc,dev);
    gtk_widget_show_all(window);
    gtk_main();
    return 0;
}

