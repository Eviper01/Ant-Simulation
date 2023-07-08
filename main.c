#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#define Simulation_Steps 1000


//Ant States
#define Ant_Foraging 0
#define Ant_Homing 1

//Ant Parameters
#define Ant_View_Range 100
#define Ant_View_Angle 0.3 //Radians
#define Ant_Interact_Range 10                           
#define Ant_Movement_Range 10
#define Ant_Mem_Size 41 //8 byte pointer to next ant, 1 byte state, 8 byte x, 8 byte y, 8 byte angle, 8 byte pointer to food object.


//Object Structs;
struct food_struct {
    struct food_struct* next;
    double xpos;
    double ypos;
    struct ant_struct* carrying;
};

struct trail_struct{
    struct trail_struct* next;
    double xpos;
    double ypos;
    double lifetime;

};

struct ant_struct{
    struct ant_struct* next;
    double xpos;
    double ypos;
    double angle;
    char state;
    struct food_struct* carrying;
};

int in_interact_range(struct ant_struct Ant_Address, struct food_struct* Food) {
   
    double Delta_X = (*Food).xpos - Ant_Address.xpos;
    double Delta_Y = Food_Y - Ant_Y;
    double Magnitude_Squared = pow(Delta_X,2) + pow(Delta_Y, 2);

    if(Magnitude_Squared < Ant_Interact_Range) {
    return 1;
    }
    return 0;
}

int64_t* init_ant(int64_t* Ants_List) {
    //creates an ant and returns a poitner to it
    int64_t* Ant_Address = Ants_List;
    while (Ant_Address != 0) {
        Ant_Address = (int64_t*)*Ant_Address;
    }
    Ant_Address = malloc(Ant_Mem_Size); //This is never freed
    //Need to assign the parameters
    return Ant_Address;
}







int in_view_cone(double Target_X, double Target_Y, double Ant_X, double Ant_Y, double Ant_Orientation, double* Target_Angle) {


    double Delta_X = Target_X - Ant_X;
    double Delta_Y = Target_Y - Ant_Y;
    double Relative_Angle = atan2(Delta_X,Delta_X);
    double Delta_Angle = fabs(Relative_Angle-Ant_Orientation); //Absoulte value
    double Magnitude_Squared = pow(Delta_X,2) + pow(Delta_Y, 2);

    if(Magnitude_Squared <  Ant_View_Range && Delta_Angle < Ant_View_Range) {

    //update the poitner to the angle
    *Target_Angle = Relative_Angle;

    return 1;

    }
    return 0;
}


void move_randomly(int64_t* Ant) {

// Move in a random way 
}

void move_direction(int64_t* Ant, double Move_Angle) {
    double Ant_X = (double)*(Ant+9);
    double Ant_Y = (double)*(Ant+17);

    *(Ant+25) = Move_Angle; //figure out how to assign the double to this value (how to do a cast assignment?)
    //create a vector of lenght movement size in the direction of the ants orientation;
    Ant_X += Ant_Movement_Range*cos(Move_Angle);
    Ant_Y += Ant_Movement_Range*sin(Move_Angle);
 
}



void ant_update(struct ant_struct* Ant_Address, struct food_struct* Foods_List, struct trail_struct* Foragings_List, struct trail_struct* Homings_List) {


    if ((*Ant_Address).state == Ant_Foraging) {
       
        struct food_struct Food_Item = *Foods_List;
        int Amount_Food = 0;
        double Average_Angle = 0;

        while(Food_Item.next != 0) {
    
            //check if we can pick up the food
            if (in_interact_range(Ant_Address, &Food_Item)) {
                // This needs to be checked that it behavees as we expect and could probably be a function in its own right.
                if (Food_Item.carrying == NULL) {
                    (*Ant_Address).state = Ant_Homing;
                    (*Ant_Address).carrying = &Food_Item; //especially this line  
                    Food_Item.carrying = Ant_Address;
                    break;
                }                
            }
            //otherwise check if we can see the food 
            double* Food_Angle = 0; //this is passed to in_view_cone?
                                    //
            if (in_view_cone(Ant_Address, &Food_Item, Food_Angle)) {
                Average_Angle = (Average_Angle * Amount_Food + (*Food_Angle))/(Amount_Food+1); 
                Amount_Food++;
            }

            Food_Item = *(Food_Item.next); // Follow the trail

        }
        //repeat the while loop with the contents



        if (Amount_Food != 0) {
        move_direction(Ant_Address, Average_Angle);  
        }

        else {

            struct trail_struct Foraging_Particle = *Foragings_List;
            double Amount_Foraging = 0;
            double Average_Angle = 0;

            while(Foraging_Particle.next != NULL) {

                double* Foraging_Angle = 0;  
            
                if (in_view_cone(Ant_Address, &Foraging_Particle, Foraging_Angle)) {
                    Average_Angle = (Average_Angle * Amount_Foraging + (*Foraging_Angle))/(Amount_Foraging+1);
                    Amount_Foraging++;           
                }
            }

            Foraging_Particle = *Foraging_Particle.next; //Follow the trail

            if(Amount_Foraging != 0) {
                move_direction(Ant_Address, Average_Angle);
            }

            else {
            move_randomly(Ant_Address); //TBC
            }
       }  
    }

    if ((*Ant_Address).state == Ant_Homing) {



        //check if you can see a colony 
        //
        //Range detect -- calculate center to ant displacement magnitude < ant_view_distance + colony_size
        //In FOV  
        //
        //

        //otherwise look for trails
 
        int64_t* Homing_Address = Homings_List;
        double Amount_Homing = 0;
        double Average_Angle = 0;     

        while(*Homing_Address != 0) {
        
            double* Homing_Angle = 0;  
            double Homing_X = (double)*(Homing_Address + 8);
            double Homing_Y = (double)*(Homing_Address + 16);
            
            if (in_view_cone(Homing_X, Homing_Y, Ant_X, Ant_Y, Ant_Orientation, Homing_Angle)) {
                    Average_Angle = (Average_Angle * Amount_Homing + (*Homing_Angle))/(Amount_Homing+1);
                    Amount_Homing++;           
                }

            Homing_Address = (int64_t*)*Homing_Address; //Follow the trail
          
        if(Amount_Homing != 0) {
            move_direction(Ant_Address, Average_Angle);
        }
        else{
            move_randomly(Ant_Address); //TBC
        }


        }
    }
}




int64_t *setup() {
// Load the Initial State

        
        setup_struct* Setup_Data = malloc(10);
        return Setup_Data;
}

void loop(struct ant_struct* Ants_List, struct trail_struct* Homings_List, struct trail_struct* Foragings_List, struct food_struct* Foods_List) {
// Loop through the objects

// Linked List of Ants - {uint8_t: State, float2: position, float: orientation, ptr: Food}
// Colony location - {float2: center, float: radius}

// Linked List of Trail Particles - {float2: position, float: lifetime}
//Homing Trails
//Food Trails




// Linked List of Food Particles - {float2: Normal_position, ptr: Ant} - there needs to be a handshake between this.
//
// Set of Walls in the Level (Investigate what datatype to use)

    //the variables could be renamined to a more discreptive type

    struct ant_struct Ant_Address = *Ants_List;
    while (Ant_Address.next != NULL) {
        //Fix the pointer types
        ant_update(&Ant_Address, Foods_List, Foragings_List, Homings_List);
        Ant_Address = *Ant_Address.next;
    }
    //!!!Perform the loop contentx explicitly on the last element
    // use a last condition in the hwile loop to perfomr this
    struct trail_struct Homing_Address = *Homings_List;
    while (Homing_Address.next != NULL) {
        //trail_update(Homing_Address);
        Homing_Address = *Homing_Address.next;
    }
    //!!!Perform the loop contentx explicitly on the last element
    
    struct trail_struct Foraging_Address = *Foragings_List;
    while (Foraging_Address.next != NULL) {
        //trail_update(Foraging_Address);
        Foraging_Address = *Foraging_Address.next;
    }
    //!!!Perform the loop contentx explicitly on the last element



}


int main() {

    setup_struct* Setup_Data = setup();

    //Elaborate setupdata




    for (int i = 0; i < Simulation_Steps; i++) {
//        loop(Ants_List);
    }
    return 0;
}
