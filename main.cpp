#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <SFML/Graphics.hpp>



//constants
#define M_PI 3.14159265358979323846  /* pi */

//Simulation Parameters
#define Number_Ants 20
#define Canvas_X 1280
#define Canvas_Y 720
#define Number_Foods 50

//field/trail/horomone behaviour
#define Trail_Increment 1.0 //raw amount that gets added by each ant
#define Trail_Decay 0.8   //percent left per tick
#define Trail_Diffuse 0.1 //percent after decay that spreads to the adjacenet nodes

//Ant States
#define Ant_Foraging 0
#define Ant_Homing 1

//Ant Parameters
#define Ant_View_Range 20
#define Ant_Sense_Range 0
#define Ant_View_Angle 0.4 //Radians
#define Ant_Interact_Range 5                           
#define Ant_Movement_Range 1


//logic
#define Type_Food 0
#define Type_Colony 1

//ObjectStructs:
struct food_struct {
    struct food_struct* next;
    struct food_struct* last;
    double xpos;
    double ypos;
    struct ant_struct* carrying;
};

struct ant_struct{
    struct ant_struct* next;
    double xpos;
    double ypos;
    double angle;
    char state;
    struct food_struct* carrying;
};

struct colony_struct {
    struct colony_struct* next;
    double xpos;
    double ypos; 
    double radius;
};

struct setup_struct{
    struct ant_struct*      ants_list;
    struct food_struct*     foods_list;
    struct colony_struct*   colonys_list;
};

//MathHelpers:
double isValueInRange(double value, double xbound1, double xbound2) {
    // Check if xbound1 and xbound2 are in numerical order
    if (xbound1 < xbound2) {
        // Check if value is between xbound1 and xbound2
        if (value >= xbound1 && value <= xbound2) {
            return 1;  // Value is in the range
        }
    } else {
        // Check if value is between xbound2 and xbound1
        if (value >= xbound2 && value <= xbound1) {
            return 1;  // Value is in the range
        }
    }
    
    return 0;  // Value is not in the range
}

double randfrom(double min, double max) 
{
    double range = (max - min); 
    double div = RAND_MAX / range;
    return min + (rand() / div);
}

//DataInits:
struct food_struct* init_food(struct food_struct** Foods_List, double xpos, double ypos) {
    
    if(*Foods_List == NULL) {
        *Foods_List = (struct food_struct*)malloc(sizeof(struct food_struct)); 
        (*Foods_List)->next = NULL;
        (*Foods_List)->last = NULL;
        (*Foods_List)->xpos = xpos;
        (*Foods_List)->ypos = ypos;
        (*Foods_List)->carrying = NULL;
        return *Foods_List;
    }

    struct food_struct* Food_Address = *Foods_List;
    while ((*Food_Address).next != NULL) { 
        Food_Address = (*Food_Address).next;
    }
    (*Food_Address).next =  (struct food_struct*)malloc(sizeof(struct food_struct)); //This is never freed
    (*(*Food_Address).next).next = NULL;
    (*(*Food_Address).next).xpos = xpos;
    (*(*Food_Address).next).ypos = ypos;
    (*(*Food_Address).next).carrying = NULL;
    (*(*Food_Address).next).last = Food_Address;
    return (*Food_Address).next;


}

struct colony_struct* init_colony(struct colony_struct** Colonys_List, double xpos, double ypos, double radius) {

    if(*Colonys_List == NULL) {
        *Colonys_List = (struct colony_struct*)malloc(sizeof(struct colony_struct)); 
        (*Colonys_List)->next = NULL;
        (*Colonys_List)->xpos = xpos;
        (*Colonys_List)->ypos = ypos;
        (*Colonys_List)->radius = radius;
        return *Colonys_List;
    }

    struct colony_struct* Colony_Address = *Colonys_List;
    while ((*Colony_Address).next != NULL) {
        Colony_Address = (*Colony_Address).next;
    }
    (*Colony_Address).next =  (struct colony_struct*)malloc(sizeof(struct colony_struct)); //This is never freed
    (*(*Colony_Address).next).next = NULL;
    (*(*Colony_Address).next).xpos = xpos;
    (*(*Colony_Address).next).ypos = ypos;
    (*(*Colony_Address).next).radius = radius;
    return (*Colony_Address).next;
}
struct ant_struct* init_ant(struct ant_struct** Ants_List, double xpos, double ypos) {
    //creates an ant and returns a poitner to it as well as adding it to the list

    if(*Ants_List == NULL) {
        *Ants_List = (struct ant_struct*)malloc(sizeof(struct ant_struct)); 
        (*Ants_List)->next = NULL;
        (*Ants_List)->state = Ant_Foraging;
        (*Ants_List)->angle = randfrom(0, 2*M_PI);
        (*Ants_List)->xpos = xpos;
        (*Ants_List)->ypos = ypos;
        return *Ants_List;
    }

    struct ant_struct* Ant_Address = *Ants_List;
    while ((*Ant_Address).next != NULL) {
        Ant_Address = (*Ant_Address).next;
    }
    (*Ant_Address).next =  (struct ant_struct*)malloc(sizeof(struct ant_struct)); //This is never freed
    (*(*Ant_Address).next).next = NULL;
    (*(*Ant_Address).next).state = Ant_Foraging;
    (*(*Ant_Address).next).angle = randfrom(0, 2*M_PI);
    (*(*Ant_Address).next).xpos = xpos;
    (*(*Ant_Address).next).ypos = ypos;
    return (*Ant_Address).next;
}

//ObjectInteractions:
int in_sense_range(struct ant_struct* Ant, void* Object, double* Target_X, double* Target_Y, double* Distance, int type) {

    if (type == Type_Food) {
        struct food_struct* Food = (struct food_struct*)Object;
        double Delta_X = Food->xpos - Ant->xpos;
        double Delta_Y = Food->ypos - Ant->ypos;
        double Magnitude_Squared = pow(Delta_X, 2) + pow(Delta_Y, 2);
        *Distance = Magnitude_Squared;
        
        *Target_X = Delta_X;
        *Target_Y = Delta_Y;
        
        if(Magnitude_Squared < pow(Ant_Sense_Range, 2)) {
            return 1;
        }
        return 0;
    }

    if(type == Type_Colony) {
        struct colony_struct* Colony = (struct colony_struct*)Object; 
        double Delta_X = Colony->xpos - Ant->xpos;
        double Delta_Y = Colony->ypos - Ant->ypos;
        double Magnitude_Squared = pow(Delta_X, 2) + pow(Delta_Y, 2);
        if (Magnitude_Squared < pow(Ant_Interact_Range + Colony->radius, 2)) {
            return 1;
        }
        return 0;
    }

    return -1;
}

int in_interact_range(struct ant_struct* Ant_Address, void* Object, int type) {
    
    if(type == Type_Food) {
        struct food_struct* Food = (struct food_struct*)Object;
        double Delta_X = (*Food).xpos - (*Ant_Address).xpos;
        double Delta_Y = (*Food).ypos - (*Ant_Address).ypos;
        double Magnitude_Squared = pow(Delta_X,2) + pow(Delta_Y, 2);
        if(Magnitude_Squared < pow(Ant_Interact_Range, 2)) {
            return 1;
        }
        return 0;
    }
    if(type == Type_Colony) {
       struct colony_struct* Colony = (struct colony_struct*)Object;
        double Delta_X = (*Colony).xpos - (*Ant_Address).xpos;
        double Delta_Y = (*Colony).ypos - (*Ant_Address).ypos;
        double Magnitude_Squared = pow(Delta_X,2) + pow(Delta_Y, 2);
        if(Magnitude_Squared < pow(Colony->radius,2)) {
            return 1;
        }
        return 0;  
    }
    return -1;
}

int pickup_food(struct ant_struct* Ant, struct food_struct* Food) {
    Ant->state = Ant_Homing;
    Ant->carrying = Food; 
    Food->carrying = Ant;
    return 0;
}

int in_view_cone(struct ant_struct* Ant_Address, void* Object, double* Target_X, double* Target_Y, double* Distance, int type) {

    if (type == Type_Food) {
        struct food_struct* Food = (struct food_struct*)Object;
        double Delta_X = (*Food).xpos - (*Ant_Address).xpos;
        double Delta_Y = (*Food).ypos - (*Ant_Address).ypos;
        double Relative_Angle = atan2(Delta_Y,Delta_X);
        double Delta_Angle = fabs(Relative_Angle-(*Ant_Address).angle); //Absoulte value
        double Magnitude_Squared = pow(Delta_X,2) + pow(Delta_Y, 2);
        *Distance = Magnitude_Squared;
        if((Magnitude_Squared <  pow(Ant_View_Range,2)) && (Delta_Angle < Ant_View_Angle)) {
            *Target_X = Delta_X;
            *Target_Y = Delta_Y;
            return 1;

        }
        return 0;
    }

    if (type == Type_Colony) {
        //This will be square detection
        struct colony_struct* Colony = (struct colony_struct*)Object;
        double Delta_X = (*Colony).xpos - (*Ant_Address).xpos;
        double Delta_Y = (*Colony).ypos - (*Ant_Address).ypos;
        double Relative_Angle = atan2(Delta_Y,Delta_X);
        double Delta_Angle = fabs(Relative_Angle-(*Ant_Address).angle); //Absoulte value
        double Magnitude_Squared = pow(Delta_X,2) + pow(Delta_Y, 2);

        if(Magnitude_Squared <  pow(Ant_View_Range + Colony->radius, 2) && Delta_Angle < Ant_View_Angle) {
            //update the poitner to the angle
            *Target_X = Delta_X;
            *Target_Y = Delta_Y;
            return 1;

        }
       return 0;

    }
    return -1; //throw an error
}


void move_xy(struct ant_struct* Ant, double xRel, double yRel) {

    double Length_Squared = pow(xRel, 2) + pow(yRel, 2);
    double Correction_Factor = sqrt(Ant_Movement_Range/Length_Squared);
    Ant->xpos += xRel*Correction_Factor;
    Ant->ypos += yRel*Correction_Factor;
    Ant->angle = atan2(yRel, xRel);
}


void move_direction(struct ant_struct* Ant, double Move_Angle) {
    Ant->angle = Move_Angle;
    Ant->xpos = Ant->xpos +  (double)Ant_Movement_Range*cos(Move_Angle);
    Ant->ypos = Ant->ypos + (double)Ant_Movement_Range*sin(Move_Angle);
}

void move_randomly(struct ant_struct* Ant) {
   
    double Move_Angle = randfrom(-Ant_View_Angle, Ant_View_Angle);
    move_direction(Ant, Ant->angle + Move_Angle);
}

void delete_object(void* Object, int type){
    //this causes a segfault
    if(type == Type_Food) {
        struct food_struct* Food = (struct food_struct*)Object;
        (Food->next)->last = Food->last;
        (Food->last)->next = Food->next;
        free(Food);
    }
}

int render_ant(struct ant_struct* Ant, sf::RenderWindow* window) {

    //Render the View Cone
    sf::ConvexShape convex;
    convex.setPointCount(3);
    //must render all clockwise/ccw
    convex.setPoint(0, sf::Vector2f(Ant->xpos, Ant->ypos));

    float angle1 = Ant->angle - Ant_View_Angle;
    float angle2 = Ant->angle + Ant_View_Angle;

    if (angle1 > angle2) {
        std::swap(angle1, angle2);
    }

    convex.setPoint(1, sf::Vector2f(Ant->xpos + Ant_View_Range * cos(angle1), Ant->ypos + Ant_View_Range * sin(angle1)));
    convex.setPoint(2, sf::Vector2f(Ant->xpos + Ant_View_Range * cos(angle2), Ant->ypos + Ant_View_Range * sin(angle2)));



    convex.setFillColor(sf::Color(255, 0, 0));
    window->draw(convex);

    if (Ant->state == Ant_Foraging) {
        sf::CircleShape shape(3);
        shape.setOrigin(1.5, 1.5);
        shape.setFillColor(sf::Color(250, 0, 250));
        shape.setPosition(Ant->xpos, Ant->ypos);
        window->draw(shape);
        return 1;

    }
    if (Ant->state == Ant_Homing) {
        sf::CircleShape shape(3);
        shape.setOrigin(1.5, 1.5);
        shape.setFillColor(sf::Color(0, 0, 250));
        shape.setPosition(Ant->xpos, Ant->ypos);
        window->draw(shape);
        return 1;
    }
    return 0;
}

int render_food(struct food_struct* Food, sf::RenderWindow* window) {
    if (Food->carrying == NULL) {
        sf::CircleShape shape(3);
        shape.setOrigin(1.5, 1.5);
        shape.setFillColor(sf::Color(0, 255, 0));
        shape.setPosition(Food->xpos, Food->ypos);
        window->draw(shape);
        return 0;
    }
    sf::CircleShape shape(3);
    shape.setOrigin(1.5, 1.5);
    shape.setFillColor(sf::Color(0, 255, 0));
    shape.setPosition((Food->carrying)->xpos, (Food->carrying)->ypos);
    window->draw(shape);
    return 0;
}

int render_colony(struct colony_struct* Colony, sf::RenderWindow* window) {
    sf::CircleShape shape(Colony->radius);
    shape.setOrigin(Colony->radius/2, Colony->radius/2); 
    shape.setFillColor(sf::Color(255, 0, 0));
    shape.setPosition(Colony->xpos, Colony->ypos);
    window->draw(shape);
    return 0;
}

int ant_update(struct ant_struct* Ant_Address, struct food_struct* Foods_List,  double* Foraging_Field, double* Homing_Field, struct colony_struct* Colonys_List) {

    //Ant needs to be dropping trails depending on its state
    //
    //
    
    
    
    if ((*Ant_Address).state == Ant_Foraging) {


        *(Foraging_Field + Canvas_X*((int)Ant_Address->xpos) + (int)Ant_Address->ypos) += Trail_Increment;


        //check if food is left in the level
        if (Foods_List == NULL) {
            printf("No Food\n");
            return 0;
        } 

        struct food_struct* Food_Item = Foods_List;
        double Least_Mag_Squared = -1;
        double Move_X = 0;
        double Move_Y = 0;
        int Looping = 1;
        while(Looping) {

            //check if we can pick up the food
            if (in_interact_range(Ant_Address, Food_Item, Type_Food) && Food_Item->carrying == NULL) {
                pickup_food(Ant_Address, Food_Item);
                return 1;
            }
            double Food_Rel_X = 0;
            double Food_Rel_Y = 0;
            double Distance = 0;
            if ((in_view_cone(Ant_Address, Food_Item, &Food_Rel_X, &Food_Rel_Y, &Distance, Type_Food) || in_sense_range(Ant_Address, Food_Item, &Food_Rel_X, &Food_Rel_Y, &Distance, Type_Food)) && Food_Item->carrying == NULL) {
                if(Distance < Least_Mag_Squared || Least_Mag_Squared < 0) {
                    Least_Mag_Squared = Distance;
                    Move_X = Food_Rel_X;
                    Move_Y = Food_Rel_Y;
                }

            }
            if (Food_Item->next != NULL) {
                Food_Item = (Food_Item->next); // Follow the trail
            }
            else {
                Looping = 0; //Terminate the loop 
            }
        }

        if (Least_Mag_Squared > 0) {
            move_xy(Ant_Address, Move_X, Move_Y); 
            return 1;
        }
        


        //determine which direction has the highest hormone strenght

        //could not find them so

        move_randomly(Ant_Address);
        return 1;

    }

    if ((*Ant_Address).state == Ant_Homing) {

        
        *(Homing_Field + Canvas_X*((int)Ant_Address->xpos) + (int)Ant_Address->ypos) += Trail_Increment;
        //Check if you can see a colony
        struct colony_struct* Colony = Colonys_List;
        double Homing_Angle;
        double Move_Angle;
        double Max_Squared_Distance = -1;
        int Looping = 1;
        int Found = 0;

        double Target_X;
        double Target_Y;
        double Distance;

        while(Looping) {

            if(in_interact_range(Ant_Address, Colony, Type_Colony)) {

                delete_object(Ant_Address->carrying, Type_Food);
                Ant_Address->carrying = NULL; 
                Ant_Address->state = Ant_Foraging; 

                return 1;
            }
            
            //then check if you can see some colonies
            if(in_view_cone(Ant_Address, Colony, &Target_X, &Target_Y, &Distance, Type_Colony)) {
                move_xy(Ant_Address, Target_X, Target_Y); 
            }

            
            if(Colony->next == NULL){
                Looping = 0;
            }
            else {
                Colony = Colony->next;
            }

        }

        //could not find any colonies so now look for trails

        
        //move the direction with the highest strenght



        //Could not see anything so
        move_randomly(Ant_Address);
        return 1;

    }
    return -1;
}

struct setup_struct* setup() {
    struct setup_struct* Setup_Data = (struct setup_struct*)malloc(sizeof(struct setup_struct));
    Setup_Data->ants_list = NULL;
    for (int i = 0 ; i < Number_Ants; i++) {
        init_ant(&(Setup_Data->ants_list), Canvas_X/2, Canvas_Y/2);
    }
    Setup_Data->colonys_list = NULL;
    struct colony_struct* Colony = init_colony(&(Setup_Data->colonys_list), Canvas_X/2, Canvas_Y/2, Canvas_X*Canvas_Y/60000);
    Setup_Data->foods_list = NULL;
    double Cluster_X = randfrom(0, Canvas_X);
    double Cluster_Y = randfrom(0, Canvas_Y);
    for (int i = 0 ; i < Number_Foods; i++) {
        struct food_struct* Food = init_food(&(Setup_Data->foods_list), Cluster_X + randfrom(-5,5), Cluster_Y + randfrom(-5, 5));
    }
    return Setup_Data;
}

void loop(sf::RenderWindow* window, struct ant_struct** Ants_List, double* Foraging_Field, double* Homing_Field, struct food_struct** Foods_List, struct colony_struct** Colonys_List) {

    
    // Set of Walls in the Level (Investigate what datatype to use)
    
    //ants
    struct ant_struct* Ant_Address = *Ants_List;
    while (Ant_Address->next != NULL) {
        ant_update(Ant_Address, *Foods_List, Foraging_Field, Homing_Field, *Colonys_List);
        render_ant(Ant_Address, window);
        //draw the ant
        Ant_Address = Ant_Address->next;
    }
    ant_update(Ant_Address, *Foods_List, Foraging_Field, Homing_Field, *Colonys_List);
    render_ant(Ant_Address, window);

    //foods
    struct food_struct* Food_Address = *Foods_List;
    while (Food_Address->next != NULL) {
        render_food(Food_Address, window);
        Food_Address = Food_Address->next;
    }
    render_food(Food_Address, window);
 
    struct colony_struct* Colony_Address = *Colonys_List;
    while (Colony_Address->next != NULL) {
        render_colony(Colony_Address, window);
        Colony_Address = Colony_Address->next;
    }
    render_colony(Colony_Address, window);


    //Command to save the state

}


int main() {

    srand((unsigned int)time(NULL));
    struct setup_struct* Setup_Data = setup();
    
    //window init
    sf::RenderWindow window(sf::VideoMode(Canvas_X, Canvas_Y) , "Ant Simulation");
    window.setVerticalSyncEnabled(true); // call it once, after creating the window
    window.setActive(true);
    window.setFramerateLimit(60); //could have somethign like microstepping in this


    //Elaborate setupdata
    struct ant_struct* Ants_List = (*Setup_Data).ants_list;
    struct food_struct* Foods_List = (*Setup_Data).foods_list;
    struct colony_struct* Colonys_List = (*Setup_Data).colonys_list;


    double* Foraging_Field  = (double*)malloc(Canvas_X * Canvas_X * sizeof(double));
    double* Homing_Field = (double*)malloc(Canvas_X * Canvas_X * sizeof(double));

    int i = 0;
    while (window.isOpen())
    {
        sf::Event event;
        while (window.pollEvent(event))
        {
            switch (event.type)
            {
            case sf::Event::Closed:
                window.close();
                //run the exit routine otherwise
                break;
            default:
                break;
            }

        }
        // clear the window with black color
        window.clear(sf::Color::Black);
//        printf("Looping %d\n", i);
        //loop function should deal with window 
        loop(&window, &Ants_List, Foraging_Field, Homing_Field, &Foods_List, &Colonys_List);
        i++; 
             
        // end the current frame
        window.display();



    }
    return 0;
}
