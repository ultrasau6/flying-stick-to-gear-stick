//#include <cstdint>
#include <math.h>
#include <stdio.h>
#include <fcntl.h>

#include <stdlib.h>
#include <unistd.h>    // close(), read()
#include <string.h>    // strerror()

#include <libevdev-1.0/libevdev/libevdev.h>
#include <libevdev-1.0/libevdev/libevdev-uinput.h>

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
        //printf("Xpos R : %d\n",Xpos);
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


int main() {
    struct libevdev *dev = NULL;
    const char* name = malloc(100);
    char* path = malloc(100);

    int fd ;
    int rc ;
    int perNum = 0;

    while (1)
    {
        perNum++;
        sprintf(path, "/dev/input/event%d", perNum);
        // 1. Ouvrir le fichier du périphérique
        fd = open(path, O_RDWR | O_NONBLOCK);
        if (fd < 0) {
            perror("open");
            return 1;
        }
        // 2. Créer une struct libevdev à partir du fd
        rc = libevdev_new_from_fd(fd, &dev);
        if (rc < 0) {
            fprintf(stderr, "Erreur libevdev: %s\n", strerror(-rc));
            return 1;
        }

        // 3. Afficher le nom du périphérique (vérification)
        name = libevdev_get_name(dev);
        //printf("Device: %s\n", name);
        if (strcmp(name,"Microsoft SideWinder Force Feedback 2 Joystick") == 0){
            printf("finded the joystick\n");
            break;
        }
        // 4. Nettoyage
        libevdev_free(dev);
        close(fd);

    }

    struct libevdev *vdev = libevdev_new();
    libevdev_set_name(vdev, "virtual-shifter");

    libevdev_enable_event_type(vdev, EV_KEY);
    for (int i = 0; i < 8; i++) {
        libevdev_enable_event_code(vdev, EV_KEY, BTN_TRIGGER_HAPPY1 + i, NULL);
    }

    
    int rc2 = libevdev_uinput_create_from_device(vdev,LIBEVDEV_UINPUT_OPEN_MANAGED,&uidev);
    if (rc2 < 0) {
        fprintf(stderr, "Erreur uinput: %s\n", strerror(-rc));
        return 1;
    }
    

    readRControl(rc,dev);
    return 0;
}

