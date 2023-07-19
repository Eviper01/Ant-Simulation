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
#define Number_Ants 75
#define Canvas_X 1280
#define Canvas_Y 720
#define Number_Clusters 5
#define Number_Foods 25
#define Diffusion 0

//Rendering
#define Rendering_View_Cone 0
#define Rendering_Foraging_Trail 1
#define Rendering_Homing_Trail 1
#define FrameRate 60

//field/trail/horomone behaviour
#define Trail_Increment 1.0 //raw amount that gets added by each ant
#define Trail_Decay 0.9975  //percent left per tick
#define Trail_Cutoff 0.2
#define Trail_Aging 1.0
#define Trail_Maturity 1.0
#define Trail_Diffuse 0.005 //percent after decay that spreads to the adjacenet nodes
#define Diffusion_Range 1 // size of the square we diffuse into

//Ant States
#define Ant_Foraging 0
#define Ant_Homing 1

//Ant Parameters
#define Ant_View_Range 40
#define Ant_Sense_Range 30 //this has a huge performance hit
#define Ant_Move_Variance 0.3 //angle in radians
#define Ant_View_Angle 0.4 //Radians
#define Ant_Interact_Range 8                           
#define Ant_Movement_Range 3

//logic
#define Type_Food 0
#define Type_Colony 1
#define Type_Foraging 2
#define Type_Homing 3

//ObjectStructs:
//
//TODO: food list should be shorted into a 2d linked list of food objects that is sorted by x and y position
struct food_struct {
    struct food_struct* next;
    struct food_struct* last;
    double xpos;
    double ypos;
    struct ant_struct* carrying;
};
//TODO: some system where the ant can lose interest in strong chemicals if its found a stronger one recenty
struct ant_struct{
    struct ant_struct* next;
    double xpos;
    double ypos;
    double angle;
    char state;
    struct food_struct* carrying;
    double timer;
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
double logisticCurve(double value) {

    return 1.0 / (1+ exp(-value));

}

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

int is_point_in_triangle(int x, int y, int p1x, int p1y, int p2x, int p2y, int p3x, int p3y) {

    int v0x = p3x - p1x;
    int v0y = p3y - p1y;
    int v1x = p2x - p1x;
    int v1y = p2y - p1y;
    int v2x = x - p1x;
    int v2y = y - p1y;

    int dot00 = v0x * v0x + v0y * v0y;
    int dot01 = v0x * v1x + v0y * v1y;
    int dot02 = v0x * v2x + v0y * v2y;
    int dot11 = v1x * v1x + v1y * v1y;
    int dot12 = v1x * v2x + v1y * v2y;

    if (dot00 * dot11 - dot01*dot01 == 0) {
        return 0;
    }
    int inv_denom = 1 / (dot00 * dot11 - dot01 * dot01);
    int u = (dot11 * dot02 - dot01 * dot12) * inv_denom;
    int v = (dot00 * dot12 - dot01 * dot02) * inv_denom;

    return (u >= 0 && v >= 0 && u + v <= 1);
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
        (*Ants_List)->timer = Trail_Increment;
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
    (*(*Ant_Address).next).timer = Trail_Increment;
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
    //TODO: place the ant into the no food state when there is not food left
    Ant->state = Ant_Homing;
    Ant->timer = Trail_Increment;
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

void enforce_boundary(struct ant_struct* Ant) {

    if (Ant->xpos > Canvas_X - 2*Ant_View_Range) {
        Ant->xpos = Canvas_X - 2*Ant_View_Range;
        Ant->angle = randfrom(0, 2*M_PI);
    }
    if (Ant->ypos > Canvas_Y - 2*Ant_View_Range) {
        Ant->ypos = Canvas_Y - 2*Ant_View_Range;
        Ant->angle = randfrom(0, 2*M_PI);
    }
    if (Ant->xpos < 2*Ant_View_Range) {
        Ant->xpos = 2*Ant_View_Range;
        Ant->angle = randfrom(0, 2*M_PI);
    }
    if (Ant->ypos < 2*Ant_View_Range) {
        Ant->ypos = 2*Ant_View_Range;
        Ant->angle = randfrom(0, 2*M_PI);
    }
}

void move_xy(struct ant_struct* Ant, double xRel, double yRel) {

    double Length_Squared = pow(xRel, 2) + pow(yRel, 2);
    double Correction_Factor = sqrt(Ant_Movement_Range/Length_Squared);
    Ant->xpos += xRel*Correction_Factor;
    Ant->ypos += yRel*Correction_Factor;
    Ant->angle = atan2(yRel, xRel);
    enforce_boundary(Ant); 

}


void move_direction(struct ant_struct* Ant, double Move_Angle) {
    Ant->angle = Move_Angle;
    Ant->xpos = Ant->xpos +  (double)Ant_Movement_Range*cos(Move_Angle);
    Ant->ypos = Ant->ypos + (double)Ant_Movement_Range*sin(Move_Angle);
    enforce_boundary(Ant); 

}

void move_randomly(struct ant_struct* Ant) {
   
    double Move_Angle = randfrom(-Ant_Move_Variance, Ant_Move_Variance);
    move_direction(Ant, Ant->angle + Move_Angle);
}

void delete_object(void* Object, void**List, int type){
    if(type == Type_Food) {
        struct food_struct* Food = (struct food_struct*)Object;
         
        //deleting zero breaks the system
        if (Food->next != NULL) {
            (Food->next)->last = Food->last;
        }
        if (Food->last != NULL) {
            (Food->last)->next = Food->next;
        }
        else {
            *List = Food->next; //updating the head //causes a seg fault
        }
        free(Food);
    }
}

int render_ant(struct ant_struct* Ant, sf::RenderWindow* window) {


    if(Rendering_View_Cone) {
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
        convex.setFillColor(sf::Color(255, 0, 0, 128));
        window->draw(convex);
    }
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
    shape.setOrigin(Colony->radius, Colony->radius); 
    shape.setFillColor(sf::Color(255, 0, 0));
    shape.setPosition(Colony->xpos, Colony->ypos);
    window->draw(shape);
    return 0;
}


int diffuse_field(double* Field) {
        
    double* Diffuse_Field = (double*)malloc(Canvas_X * Canvas_Y * sizeof(double));

    for(int Col = 0; Col < Canvas_X; Col++) {
        for(int Row = 0; Row < Canvas_Y; Row++) {
            int Main_Index = Canvas_X*Row + Col;
            for (int OffsetX = -Diffusion_Range; OffsetX <= - Diffusion_Range; OffsetX++) {
                for (int OffsetY = -Diffusion_Range; OffsetY <= - Diffusion_Range; OffsetY++) {
                    int Index = Canvas_X*(Row + OffsetY) + (Col + OffsetX);
                    if(Index > 0 && Index < Canvas_X*Canvas_Y && OffsetX*OffsetY != 0) {
                    *(Diffuse_Field + Index) += Trail_Diffuse* *(Field + Main_Index);
                    }
                } 
            }

        }
    }
    for(int Col = 0; Col < Canvas_X; Col++) {
        for(int Row = 0; Row < Canvas_Y; Row++) {
            int Main_Index = Canvas_X*Row + Col;
            *(Field + Main_Index) += *(Diffuse_Field + Main_Index);
        }
    }
    free(Diffuse_Field);
    return 0;
}




int renderAndupdate_field(double* Field_Intensity, double* Field_Maturity,  int type, sf::RenderWindow* window) {
    
    for (int Col = 0; Col < Canvas_X; Col++ ) {
        for(int Row = 0; Row < Canvas_Y; Row++) {
            int Index = Canvas_X*Row + Col;
            double* Intensity = (Field_Intensity + Index);
            *Intensity *= Trail_Decay*Trail_Decay;

            double* Maturity  = (Field_Maturity + Index);
            if (*Intensity > Trail_Cutoff) {
                *Maturity += Trail_Aging; 
                if (type == Type_Foraging && Rendering_Foraging_Trail) {
                    sf::RectangleShape rectangle(sf::Vector2f(1.0, 1.0));
                    rectangle.setPosition(Col, Row);
                    rectangle.setFillColor(sf::Color(0, 255, 0, 128 * logisticCurve(*(Maturity)) ));
                    window->draw(rectangle);
                } 
                if (type == Type_Homing && Rendering_Homing_Trail) {
                    sf::RectangleShape rectangle(sf::Vector2f(1.0, 1.0));
                    rectangle.setPosition(Col, Row);
                    rectangle.setFillColor(sf::Color(0, 0, 255, 128 * logisticCurve(*(Maturity)) ));
                    window->draw(rectangle);
                } 

            }
            else {
            *Maturity = 0;
            }
        }
    }
    if(Diffusion) {
        diffuse_field(Field_Intensity);
        diffuse_field(Field_Maturity);
    }
    return 1;
}
//TODO: in the right plane there is some very odd pathfinding behviaour
int view_field(struct ant_struct* Ant, double* Field, double* Angle) {

    double Max_Intensity = Trail_Maturity;
    int Detected = 0;
    double x1 = Ant->xpos;
    double y1 = Ant->ypos;
    double x2 = x1 + Ant_View_Range * cos(Ant->angle + Ant_View_Angle); 
    double y2 = y1 + Ant_View_Range * sin(Ant->angle + Ant_View_Angle); 
    double x3 = x1 + Ant_View_Range * cos(Ant->angle - Ant_View_Angle); 
    double y3 = y1 + Ant_View_Range * sin(Ant->angle - Ant_View_Angle);

    //determine the rectangular bounding box of the triangle
    double min_x = x1;
    double max_x = x1;
    if (x2 < x1) {
        min_x = x2; 
    }
    else {
        max_x = x2;
    }
    if (x3 < x1) {
        min_x = x3;
    }
    if (x3 > x2) {
        max_x = x3; 
    }
    double min_y = y1;
    double max_y = y1;
    if (y2 < y1) {
        min_y = y1; 
    }
    else {
        max_y = y2;
    }
    if (y3 < y1) {
        min_y = y3; 
    }
    if(y3 > y1) {
        max_y = y3;
    }

    //Loop through these coords
    for (int Col = (int)min_x; Col <= (int)max_x; Col++) {
        for(int Row = (int)min_y;  Row <= (int)max_y; Row++) {
            if (is_point_in_triangle(Col, Row, (int)x1, (int)y1, (int)x2, (int)y2, (int)x3, (int)y3)) {
                int Index = Canvas_X*Row + Col;
                if (Index > 0 && Index < Canvas_X*Canvas_Y) {
                    double Intensity = *(Field + Index);
                    if (Intensity > Max_Intensity) {
                        double DeltaX = (double)Col - Ant->xpos;
                        double DeltaY = (double)Row - Ant->ypos;
                        double MagSquared = pow(DeltaX, 2) + pow(DeltaY, 2);
                        if (MagSquared > 3.0) {
                            Max_Intensity = Intensity;
                            *Angle = atan2(DeltaY, DeltaX);
                            Detected = 1;
                        }
                   }
                }
            }
        }
    }
    return Detected;
}



int scan_field(struct ant_struct* Ant, double* Field, double* Angle) {
    //return the angle of the most hormone concentration
    //Check a rectange around
    double max_intensity = Trail_Maturity;
    int Detected = 0;
    for (int Row = -Ant_Sense_Range; Row <= Ant_Sense_Range; Row++) {
        for (int Col = -Ant_Sense_Range; Col <= Ant_Sense_Range; Col++) {
            int Index = Canvas_X*((int)Ant->ypos + Row)+ (int)Ant->xpos + Col;
            if(Index > 0 && Index < Canvas_X*Canvas_Y) {
                double DeltaX = (double)Col;
                double DeltaY = (double)Row;
                double MagnitudeSquared = pow(DeltaX, 2) + pow(DeltaY, 2);
                if(MagnitudeSquared > 1.0) {
                    double* intensity = (Field + Index);
                    if (*intensity > max_intensity) {
                        *Angle = atan2(DeltaY, DeltaX);
                        Detected = 1;
                        max_intensity = *intensity;
                    }
                }
            }
        } 
    }
    return Detected;
}



int ant_update(struct ant_struct* Ant_Address, struct food_struct** Foods_List,  double* Foraging_Field_Intensity, double* Foraging_Field_Maturity, double* Homing_Field_Intensity, double* Homing_Field_Maturity, struct colony_struct* Colonys_List) {
    
    Ant_Address->timer *=Trail_Decay;
    if ((*Ant_Address).state == Ant_Foraging) {

        *(Homing_Field_Intensity + Canvas_X*((int)Ant_Address->ypos) + (int)Ant_Address->xpos) += Ant_Address->timer;

        //check if food is left in the level
        if (*Foods_List == NULL) {
            printf("No Food\n");
            exit(0);
            return 0;
        } 

        struct food_struct* Food_Item = *Foods_List;
        double Least_Mag_Squared = -1;
        double Move_X = 0;
        double Move_Y = 0;
        int Looping = 1;
        while(Looping) {
            
            //check if we can pick up the food
            if (in_interact_range(Ant_Address, Food_Item, Type_Food) && Food_Item->carrying == NULL) {
                pickup_food(Ant_Address, Food_Item);
                double Angle = 0;
                if (scan_field(Ant_Address, Homing_Field_Maturity, &Angle)) {
                    Ant_Address->angle = Angle; 
                }
                move_randomly(Ant_Address);
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

        double Angle = 0;
        if (view_field(Ant_Address, Foraging_Field_Maturity, &Angle)) {
            Ant_Address->angle = Angle; 
        }

        //Checking the colonies to reset timer
        Looping = 1;
        struct colony_struct* Colony = Colonys_List;
        double Placeholder = 0;
        while(Looping) {
        
            if (in_view_cone(Ant_Address, Colony, &Placeholder, &Placeholder, &Placeholder, Type_Colony)) {
                Ant_Address->timer = Trail_Increment;
            } 

            if(Colony->next == NULL){
                Looping = 0;
            }
            else {
                Colony = Colony->next;
            }

       
        }


        move_randomly(Ant_Address);
        return 1;

    }

    if ((*Ant_Address).state == Ant_Homing) {


        *(Foraging_Field_Intensity + Canvas_X*((int)Ant_Address->ypos) + (int)Ant_Address->xpos) += Ant_Address->timer;
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

                delete_object(Ant_Address->carrying, (void**)Foods_List, Type_Food);
                Ant_Address->carrying = NULL; 
                Ant_Address->state = Ant_Foraging;
                Ant_Address->timer = Trail_Increment;
                double Angle = 0;
                if (scan_field(Ant_Address, Foraging_Field_Maturity, &Angle)) {
                    Ant_Address->angle = Angle; 
                }
                move_randomly(Ant_Address);
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
        double Angle = 0;
        if (view_field(Ant_Address, Homing_Field_Maturity, &Angle)) {
            Ant_Address->angle = Angle; 
        }

        //check if you can see some food
        if (*Foods_List != NULL) {
            Looping = 1;
            struct food_struct* Food_Item = *Foods_List;
            double Placeholder = 0;
            while (Looping) {
                
                if (in_view_cone(Ant_Address, Food_Item, &Placeholder, &Placeholder, &Placeholder, Type_Food)) {
                    Ant_Address->timer = Trail_Increment; 
                }

                if (Food_Item->next != NULL) {
                    Food_Item = (Food_Item->next); // Follow the trail 
                }
                else {
                    Looping = 0; //Terminate the loop 
                }

            } 
        }



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
    for (int i = 0; i<Number_Clusters; i++) {
        double Cluster_X = randfrom(3*Ant_View_Range, Canvas_X - 3*Ant_View_Range);
        double Cluster_Y = randfrom(3*Ant_View_Range, Canvas_Y - 3*Ant_View_Range);
        for (int i = 0 ; i < Number_Foods; i++) {
            struct food_struct* Food = init_food(&(Setup_Data->foods_list), Cluster_X + randfrom(-5,5), Cluster_Y + randfrom(-5, 5));
        }

    }
    return Setup_Data;
}

void loop(sf::RenderWindow* window, struct ant_struct** Ants_List, double* Foraging_Field_Intensity, double* Foraging_Field_Maturity, double* Homing_Field_Intensity, double* Homing_Field_Maturity, struct food_struct** Foods_List, struct colony_struct** Colonys_List) {

    // Set of Walls in the Level (Investigate what datatype to use)
    
    //ants
    struct ant_struct* Ant_Address = *Ants_List;
    while (Ant_Address->next != NULL) {
        ant_update(Ant_Address, Foods_List, Foraging_Field_Intensity, Foraging_Field_Maturity, Homing_Field_Intensity, Homing_Field_Maturity, *Colonys_List);
        render_ant(Ant_Address, window);
        //draw the ant
        Ant_Address = Ant_Address->next;
    }
    ant_update(Ant_Address, Foods_List, Foraging_Field_Intensity, Foraging_Field_Maturity, Homing_Field_Intensity, Homing_Field_Maturity, *Colonys_List);
    render_ant(Ant_Address, window);

    //foods
    struct food_struct* Food_Address = *Foods_List;
    while (Food_Address->next != NULL) {
        render_food(Food_Address, window);
        Food_Address = Food_Address->next;
    }
    render_food(Food_Address, window);
    
    //colonies
    struct colony_struct* Colony_Address = *Colonys_List;
    while (Colony_Address->next != NULL) {
        render_colony(Colony_Address, window);
        Colony_Address = Colony_Address->next;
    }


    render_colony(Colony_Address, window);
    
    //trail fields
    renderAndupdate_field(Foraging_Field_Intensity, Foraging_Field_Maturity, Type_Foraging, window);
    renderAndupdate_field(Homing_Field_Intensity, Homing_Field_Maturity, Type_Homing, window);

}


int main() {

    srand((unsigned int)time(NULL));
    struct setup_struct* Setup_Data = setup();
    
    //window init
    sf::RenderWindow window(sf::VideoMode(Canvas_X, Canvas_Y) , "Ant Simulation");
    window.setVerticalSyncEnabled(true); // call it once, after creating the window
    window.setActive(true);
    window.setFramerateLimit(FrameRate); //could have somethign like microstepping in this

    //Elaborate setupdata
    struct ant_struct* Ants_List = (*Setup_Data).ants_list;
    struct food_struct* Foods_List = (*Setup_Data).foods_list;
    struct colony_struct* Colonys_List = (*Setup_Data).colonys_list;

    double* Foraging_Field_Intensity  = (double*)malloc(Canvas_X * Canvas_X * sizeof(double));
    double* Homing_Field_Intensity = (double*)malloc(Canvas_X * Canvas_X * sizeof(double));
    double* Foraging_Field_Maturity  = (double*)malloc(Canvas_X * Canvas_X * sizeof(double));
    double* Homing_Field_Maturity = (double*)malloc(Canvas_X * Canvas_X * sizeof(double));
    
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
        //printf("Looping %d\n", i);
        loop(&window, &Ants_List, Foraging_Field_Intensity, Foraging_Field_Maturity, Homing_Field_Intensity, Homing_Field_Maturity, &Foods_List, &Colonys_List);
        i++; 
        window.display();
    }
    return 0;
}
