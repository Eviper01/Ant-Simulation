#include <SFML/Graphics.hpp>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

// constants
#define M_PI 3.14159265358979323846 /* pi */

// Simulation Parameters
#define Number_Ants 50
#define Canvas_X 800
#define Canvas_Y 600
#define Number_Clusters 8
#define Number_Foods 25
#define Diffusion 0
#define Number_Walls 1
#define Wall_Verticies 4
// Rendering
#define Rendering_View_Cone 0
#define Rendering_Foraging_Trail 1
#define Rendering_Homing_Trail 1
#define FrameRate 144
#define MicroStep 1

// field/trail/horomone behaviour
#define Trail_Increment 1.0 // raw amount that gets added by each ant
#define Trail_Decay 0.9995  // percent left per tick
#define Trail_Cutoff 0.2
#define Trail_Aging 50.0
#define Trail_Maturity 1.0
#define Trail_Diffuse 0.005 // percent after decay that spreads to the adjacenet nodes
#define Diffusion_Range 1 // size of the square we diffuse into

// Ant States
#define Ant_Foraging 0
#define Ant_Homing 1
#define Ant_Homing_NoFood 2

// Ant Parameters
#define Ant_View_Range 40
#define Ant_Sense_Range 15    // this has a huge performance hit
#define Ant_Move_Variance 0.1 // angle in radians
#define Ant_View_Angle 0.4    // Radians
#define Ant_Interact_Range 3
#define Ant_Movement_Range 1
#define Ant_Mem_Decay 0.9

// logic
#define Type_Food 0
#define Type_Colony 1
#define Type_Foraging 2
#define Type_Homing 3
#define Type_NoFood 4
// ObjectStructs:
struct food_struct {
    //Sorted by from center of the tile radius
    struct food_struct* next;
    struct food_struct* last;
    double xpos;
    double ypos;
    double radius_from_center;
    int ID;
};
struct ant_struct {
    struct ant_struct *next;
    double xpos;
    double ypos;
    double angle;
    char state;
    double timer;
    unsigned short max_intensity;
};

struct colony_struct {
    struct colony_struct *next;
    double xpos;
    double ypos;
    double radius;
};

struct wall_struct {
    double* x_points; //list of x coordinates of verticies
    double* y_points; //list of y coordinates of verticies
    int size; //amount of points in the polygon
    struct wall_struct* next;
};

struct setup_struct {
    struct ant_struct *ants_list;
    struct food_struct *foods_list;
    struct colony_struct *colonys_list;
    struct wall_struct *walls_list;
};





// MathHelpers:
double logisticCurve(unsigned short value) { return 1.0 / (1 + exp(-(double)value)); }

double isValueInRange(double value, double xbound1, double xbound2) {
    // Check if xbound1 and xbound2 are in numerical order
    if (xbound1 < xbound2) {
        // Check if value is between xbound1 and xbound2
        if (value >= xbound1 && value <= xbound2) {
            return 1; // Value is in the range
        }
    } else {
        // Check if value is between xbound2 and xbound1
        if (value >= xbound2 && value <= xbound1) {
            return 1; // Value is in the range
        }
    }

    return 0; // Value is not in the range
}

double randfrom(double min, double max) {
    double range = (max - min);
    double div = RAND_MAX / range;
    return min + (rand() / div);
}

int is_point_in_triangle(int x, int y, int p1x, int p1y, int p2x, int p2y,
        int p3x, int p3y) {

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

    if (dot00 * dot11 - dot01 * dot01 == 0) {
        return 0;
    }
    int inv_denom = 1 / (dot00 * dot11 - dot01 * dot01);
    int u = (dot11 * dot02 - dot01 * dot12) * inv_denom;
    int v = (dot00 * dot12 - dot01 * dot02) * inv_denom;

    return (u >= 0 && v >= 0 && u + v <= 1);
}

double radius_from_center(struct food_struct* Food){
 return  sqrt(pow((Food->xpos - Canvas_X/2),2) + pow((Food->ypos - Canvas_Y/2),2));

}

int ccw(double Ax, double Ay, double Bx, double By, double Cx, double Cy) {
    return (Cy-Ay)*(Bx-Ax) > (By-Ay)*(Cx-Ax);
}


int intersect(double Ax, double Ay, double Bx, double By, double Cx, double Cy, double Dx, double Dy) {
    return ccw(Ax,Ay,Cx,Cy,Dx,Dy) != ccw(Bx,By,Cx,Cy,Dx,Dy) and ccw(Ax,Ay,Bx,By,Cx,Cy) != ccw(Ax, Ay,Bx,By,Dx,Dy);
}





// DataInits and Handling:


struct food_struct* delete_food(food_struct* Food, food_struct** Food_List) {
    struct food_struct* Previous_Food;
    // deleting zero breaks the system
    if (Food->next != NULL) {
        (Food->next)->last = Food->last;
    }
    if (Food->last != NULL) {
        (Food->last)->next = Food->next;
        Previous_Food = Food->last;
    } else {
        *Food_List = Food->next;
        Previous_Food = Food->next;
    }
    free(Food);
    return Previous_Food;
}

struct wall_struct* init_wall(struct wall_struct** Walls_List, int size) {

    struct wall_struct* Wall = (struct wall_struct*)malloc(sizeof(struct wall_struct));
    Wall->x_points = (double*)malloc(size*sizeof(double));
    Wall->y_points = (double*)malloc(size*sizeof(double));
    Wall->size = size;
    Wall->next = NULL;
    int Building_Wall = 1;
    while(Building_Wall) {
        Wall->x_points[0] = randfrom(0, Canvas_X);  
        Wall->y_points[0] = randfrom(0, Canvas_Y);
        //Place all the points except the last
        for(int i = 1; i < size; i++) {
            int Generating = 1;
            while(Generating) {
                Wall->x_points[i] = randfrom(0, Canvas_X);  
                Wall->y_points[i] = randfrom(0, Canvas_Y);
                Generating = 0;
                if(i>1) {
                    double Vec1x = Wall->x_points[i-2] - Wall->x_points[i+1];
                    double Vec1y = Wall->y_points[i-2] - Wall->y_points[i+1];
                    double Vec2x = Wall->x_points[i] - Wall->x_points[i+1];
                    double Vec2y = Wall->y_points[i] - Wall->y_points[i+1];
                    double dot = (Vec1x*Vec2x+Vec1y*Vec2y); // the sign of this can be used to view the concavity
                    if( dot < 0) {
                    //TODO: Flesh this out 
                    }
                }
                for(int j = 1; j < i; j ++) {
                    double Ax = Wall->x_points[j];
                    double Ay = Wall->y_points[j];
                    double Bx = Wall->x_points[j-1];
                    double By = Wall->x_points[j-1];
                    if(intersect(Wall->x_points[i-1],Wall->y_points[i-1],Wall->x_points[i], Wall->y_points[i], Ax,Ay,Bx,By) and i != j) {
                        Generating = 1;
                    }
                }
            }
        }
        //Check if the last connects
        Building_Wall = 0;
        for(int j = 1; j < size; j ++) {
            double Ax = Wall->x_points[j];
            double Ay = Wall->y_points[j];
            double Bx = Wall->x_points[j-1];
            double By = Wall->x_points[j-1];
            if(intersect(Wall->x_points[size],Wall->y_points[size],Wall->x_points[0], Wall->y_points[0], Ax,Ay,Bx,By)) {
                Building_Wall = 1;
            }
        }
    }
    if(*Walls_List == NULL) {
        *Walls_List = Wall;
        return Wall;
    }
    struct wall_struct* Prev_Wall = *Walls_List;
    while(Prev_Wall->next != NULL) {
        Prev_Wall = Prev_Wall->next; 
    }
    Prev_Wall->next = Wall;
    return Wall;

}



struct food_struct *init_food(struct food_struct **Foods_List, double xpos, double ypos) {
    static int ID = 0;
    ID++;
    if (*Foods_List == NULL) {
        *Foods_List = (struct food_struct *)malloc(sizeof(struct food_struct));
        (*Foods_List)->next = NULL;
        (*Foods_List)->last = NULL;
        (*Foods_List)->xpos = xpos;
        (*Foods_List)->ypos = ypos;
        (*Foods_List)->ID = ID;
        (*Foods_List)->radius_from_center = radius_from_center(*Foods_List);
        return *Foods_List;
    }

    //There is already food
    //First step is to find the food with the greatest r smaller than xpos
    struct food_struct* prev_food = *Foods_List;
    double radius = sqrt(pow((xpos - Canvas_X/2),2) + pow((ypos - Canvas_Y/2),2));

    //handle the edge case of this being the first item
    if(prev_food->radius_from_center > radius) {
        struct food_struct* new_food = (struct food_struct*)malloc(sizeof(struct food_struct));
        new_food->last = NULL;
        new_food->next = prev_food;
        new_food->ID = ID;
        new_food->xpos = xpos;
        new_food->ypos = ypos;
        new_food->radius_from_center = radius_from_center(new_food);
        *Foods_List = new_food;
        return *Foods_List;
    }

    while(prev_food->next != NULL){
        if(prev_food->next->radius_from_center < radius) {
            prev_food = prev_food->next;
        }
        else{
            break;
        }
    }
    //now we have the food with the lowest radius add it to the list there
    struct food_struct* new_food = (struct food_struct*)malloc(sizeof(struct food_struct));
    struct food_struct* next_food = prev_food->next;
    if(next_food != NULL) {
        next_food->last = new_food; 
    }
    prev_food->next = new_food;
    new_food->next = next_food;
    new_food->last = prev_food;
    new_food->ID = ID;
    new_food->xpos = xpos;
    new_food->ypos = ypos;
    new_food->radius_from_center = radius_from_center(new_food);
    if(new_food->next != NULL) {
    }
    else {
    }
    return *Foods_List;

}

struct colony_struct *init_colony(struct colony_struct **Colonys_List,
        double xpos, double ypos, double radius) {

    if (*Colonys_List == NULL) {
        *Colonys_List =
            (struct colony_struct *)malloc(sizeof(struct colony_struct));
        (*Colonys_List)->next = NULL;
        (*Colonys_List)->xpos = xpos;
        (*Colonys_List)->ypos = ypos;
        (*Colonys_List)->radius = radius;
        return *Colonys_List;
    }

    struct colony_struct *Colony_Address = *Colonys_List;
    while ((*Colony_Address).next != NULL) {
        Colony_Address = (*Colony_Address).next;
    }
    (*Colony_Address).next = (struct colony_struct *)malloc(
            sizeof(struct colony_struct)); // This is never freed
    (*(*Colony_Address).next).next = NULL;
    (*(*Colony_Address).next).xpos = xpos;
    (*(*Colony_Address).next).ypos = ypos;
    (*(*Colony_Address).next).radius = radius;
    return (*Colony_Address).next;
}
struct ant_struct *init_ant(struct ant_struct **Ants_List, double xpos,
        double ypos) {
    // creates an ant and returns a poitner to it as well as adding it to the list

    if (*Ants_List == NULL) {
        *Ants_List = (struct ant_struct *)malloc(sizeof(struct ant_struct));
        (*Ants_List)->next = NULL;
        (*Ants_List)->state = Ant_Foraging;
        (*Ants_List)->timer = Trail_Increment;
        (*Ants_List)->angle = randfrom(0, 2 * M_PI);
        (*Ants_List)->xpos = xpos;
        (*Ants_List)->ypos = ypos;
        return *Ants_List;
    }

    struct ant_struct *Ant = *Ants_List;
    while ((*Ant).next != NULL) {
        Ant = (*Ant).next;
    }
    (*Ant).next = (struct ant_struct *)malloc(
            sizeof(struct ant_struct)); // This is never freed
    (*(*Ant).next).next = NULL;
    (*(*Ant).next).state = Ant_Foraging;
    (*(*Ant).next).timer = Trail_Increment;
    (*(*Ant).next).angle = randfrom(0, 2 * M_PI);
    (*(*Ant).next).xpos = xpos;
    (*(*Ant).next).ypos = ypos;
    return (*Ant).next;
}

// ObjectInteractions:
int in_sense_range(struct ant_struct *Ant, void *Object, double *Target_X,
        double *Target_Y, double *Distance, int type) {

    if (type == Type_Food) {
        struct food_struct *Food = (struct food_struct *)Object;
        double Delta_X = Food->xpos - Ant->xpos;
        double Delta_Y = Food->ypos - Ant->ypos;
        double Magnitude_Squared = pow(Delta_X, 2) + pow(Delta_Y, 2);
        *Distance = Magnitude_Squared;

        *Target_X = Delta_X;
        *Target_Y = Delta_Y;

        if (Magnitude_Squared < pow((double)Ant_Sense_Range, 2)) {
            return 1;
        }
        return 0;
    }

    if (type == Type_Colony) {
        struct colony_struct *Colony = (struct colony_struct *)Object;
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

int in_interact_range(struct ant_struct *Ant, void *Object, int type) {

    if (type == Type_Food) {
        struct food_struct *Food = (struct food_struct *)Object;
        double Delta_X = (*Food).xpos - (*Ant).xpos;
        double Delta_Y = (*Food).ypos - (*Ant).ypos;
        double Magnitude_Squared = pow(Delta_X, 2) + pow(Delta_Y, 2);
        if (Magnitude_Squared < pow((double)Ant_Interact_Range, 2)) {
            return 1;
        }
        return 0;
    }
    if (type == Type_Colony) {
        struct colony_struct *Colony = (struct colony_struct *)Object;
        double Delta_X = (*Colony).xpos - (*Ant).xpos;
        double Delta_Y = (*Colony).ypos - (*Ant).ypos;
        double Magnitude_Squared = pow(Delta_X, 2) + pow(Delta_Y, 2);
        if (Magnitude_Squared < pow(Colony->radius, 2)) {
            return 1;
        }
        return 0;
    }
    return -1;
}

int pickup_food(struct ant_struct *Ant, struct food_struct *Food) {
    Ant->state = Ant_Homing;
    Ant->timer = Trail_Increment;
    Ant->max_intensity = 0.0;
    return 0;
}

int in_view_cone(struct ant_struct *Ant, void *Object, double *Target_X,
        double *Target_Y, double *Distance, int type) {

    if (type == Type_Food) {
        struct food_struct *Food = (struct food_struct *)Object;
        double Delta_X = (*Food).xpos - (*Ant).xpos;
        double Delta_Y = (*Food).ypos - (*Ant).ypos;
        double Relative_Angle = atan2(Delta_Y, Delta_X);
        double Delta_Angle = fabs(Relative_Angle - (*Ant).angle); // Absoulte value
        double Magnitude_Squared = pow(Delta_X, 2) + pow(Delta_Y, 2);
        *Distance = Magnitude_Squared;
        if ((Magnitude_Squared < pow((double)Ant_View_Range, 2)) &&
                (Delta_Angle < Ant_View_Angle)) {
            *Target_X = Delta_X;
            *Target_Y = Delta_Y;
            return 1;
        }
        return 0;
    }

    if (type == Type_Colony) {
        // This will be square detection
        struct colony_struct *Colony = (struct colony_struct *)Object;
        double Delta_X = (*Colony).xpos - (*Ant).xpos;
        double Delta_Y = (*Colony).ypos - (*Ant).ypos;
        double Relative_Angle = atan2(Delta_Y, Delta_X);
        double Delta_Angle = fabs(Relative_Angle - (*Ant).angle); // Absoulte value
        double Magnitude_Squared = pow(Delta_X, 2) + pow(Delta_Y, 2);

        if (Magnitude_Squared < pow(Ant_View_Range + Colony->radius, 2) &&
                Delta_Angle < Ant_View_Angle) {
            // update the poitner to the angle
            *Target_X = Delta_X;
            *Target_Y = Delta_Y;
            return 1;
        }
        return 0;
    }
    return -1; // throw an error
}

void enforce_boundary(struct ant_struct *Ant) {

    if (Ant->xpos > Canvas_X) {
        Ant->xpos = Canvas_X;
        Ant->angle = randfrom(0, 2 * M_PI);
    }
    if (Ant->ypos > Canvas_Y) {
        Ant->ypos = Canvas_Y;
        Ant->angle = randfrom(0, 2 * M_PI);
    }
    if (Ant->xpos < 0) {
        Ant->xpos = 0;
        Ant->angle = randfrom(0, 2 * M_PI);
    }
    if (Ant->ypos < 0) {
        Ant->ypos = 0;
        Ant->angle = randfrom(0, 2 * M_PI);
    }
}

void enforce_walls(struct ant_struct *Ant, struct wall_struct **Walls_List) {
    //TODO: Write the code which enforces the boundary on the walls

}




void move_xy(struct ant_struct *Ant, double xRel, double yRel) {

    double Length_Squared = pow(xRel, 2) + pow(yRel, 2);
    double Correction_Factor = sqrt(Ant_Movement_Range / Length_Squared);
    Ant->xpos += xRel * Correction_Factor;
    Ant->ypos += yRel * Correction_Factor;
    Ant->angle = atan2(yRel, xRel);
    enforce_boundary(Ant);
}

void move_direction(struct ant_struct *Ant, double Move_Angle) {
    Ant->angle = Move_Angle;
    Ant->xpos = Ant->xpos + (double)Ant_Movement_Range * cos(Move_Angle);
    Ant->ypos = Ant->ypos + (double)Ant_Movement_Range * sin(Move_Angle);
    enforce_boundary(Ant);
}

void move_randomly(struct ant_struct *Ant) {

    double Move_Angle = randfrom(-Ant_Move_Variance, Ant_Move_Variance);
    move_direction(Ant, Ant->angle + Move_Angle);
}

int render_ant(struct ant_struct *Ant, sf::RenderWindow *window) {

    if (Rendering_View_Cone) {
        // Render the View Cone
        sf::ConvexShape convex;
        convex.setPointCount(3);
        // must render all clockwise/ccw
        convex.setPoint(0, sf::Vector2f(Ant->xpos, Ant->ypos));

        float angle1 = Ant->angle - Ant_View_Angle;
        float angle2 = Ant->angle + Ant_View_Angle;

        if (angle1 > angle2) {
            std::swap(angle1, angle2);
        }

        convex.setPoint(1, sf::Vector2f(Ant->xpos + Ant_View_Range * cos(angle1),
                    Ant->ypos + Ant_View_Range * sin(angle1)));
        convex.setPoint(2, sf::Vector2f(Ant->xpos + Ant_View_Range * cos(angle2),
                    Ant->ypos + Ant_View_Range * sin(angle2)));
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
        shape.setFillColor(sf::Color(0, 250, 0));
        shape.setPosition(Ant->xpos, Ant->ypos);
        window->draw(shape);
        return 1;
    }
    if(Ant->state == Ant_Homing_NoFood) {
        sf::CircleShape shape(3);
        shape.setOrigin(1.5, 1.5);
        shape.setFillColor(sf::Color(250, 0, 0));
        shape.setPosition(Ant->xpos, Ant->ypos);
        window->draw(shape);
        return 1;
    }
    return 0;
}

int render_food(struct food_struct *Food, sf::RenderWindow *window) {
    sf::CircleShape shape(3);
    shape.setOrigin(1.5, 1.5);
    shape.setFillColor(sf::Color(0, 255, 0));
    shape.setPosition(Food->xpos, Food->ypos);
    window->draw(shape);
    return 0;
    }
/*
    sf::CircleShape shape(3);
    shape.setOrigin(1.5, 1.5);
    shape.setFillColor(sf::Color(0, 255, 0));
    shape.setPosition((Food->carrying)->xpos, (Food->carrying)->ypos);
    window->draw(shape);
    return 0;
}*/

int render_colony(struct colony_struct *Colony, sf::RenderWindow *window) {
    sf::CircleShape shape(Colony->radius);
    shape.setOrigin(Colony->radius, Colony->radius);
    shape.setFillColor(sf::Color(255, 0, 0));
    shape.setPosition(Colony->xpos, Colony->ypos);
    window->draw(shape);
    return 0;
}

int render_wall(struct wall_struct* Wall, sf::RenderWindow *window) {
    sf::ConvexShape polygon;
    polygon.setPointCount(Wall->size);
    for(int i=0; i<Wall->size; i++){
        polygon.setPoint(i, sf::Vector2f(Wall->x_points[i], Wall->y_points[i]));
    }
    polygon.setFillColor(sf::Color(220,220,220));
    window->draw(polygon);
}





int diffuse_field(double *Field) {

    double *Diffuse_Field =
        (double *)malloc(Canvas_X * Canvas_Y * sizeof(double));

    for (int Col = 0; Col < Canvas_X; Col++) {
        for (int Row = 0; Row < Canvas_Y; Row++) {
            int Main_Index = Canvas_X * Row + Col;
            for (int OffsetX = -Diffusion_Range; OffsetX <= -Diffusion_Range;
                    OffsetX++) {
                for (int OffsetY = -Diffusion_Range; OffsetY <= -Diffusion_Range;
                        OffsetY++) {
                    int Index = Canvas_X * (Row + OffsetY) + (Col + OffsetX);
                    if (Index > 0 && Index < Canvas_X * Canvas_Y &&
                            OffsetX * OffsetY != 0) {
                        *(Diffuse_Field + Index) += Trail_Diffuse * *(Field + Main_Index);
                    }
                }
            }
        }
    }
    for (int Col = 0; Col < Canvas_X; Col++) {
        for (int Row = 0; Row < Canvas_Y; Row++) {
            int Main_Index = Canvas_X * Row + Col;
            *(Field + Main_Index) += *(Diffuse_Field + Main_Index);
        }
    }
    free(Diffuse_Field);
    return 0;
}

int renderAndupdate_field(double *Field_Intensity, unsigned short *Field_Maturity,
        int type, sf::RenderWindow *window, int Rendering) {
    // TODO: simd this
    sf::VertexArray verticies(sf::Points, Canvas_X * Canvas_Y);
    for (int Col = 0; Col < Canvas_X; Col++) {
        for (int Row = 0; Row < Canvas_Y; Row++) {
            int Index = Canvas_X * Row + Col;
            double* Intensity = (Field_Intensity + Index);
            *Intensity *= Trail_Decay * Trail_Decay;
            unsigned short *Maturity = (Field_Maturity + Index);
            if (*Intensity > Trail_Cutoff) {
                *Maturity += Trail_Aging;
                if (Rendering) {
                    verticies[Index].position = sf::Vector2f((float)Col, (float)Row);
                    if (type == Type_Foraging) {
                        verticies[Index].color =
                            sf::Color(0, 255, 0, 128 * logisticCurve(*(Maturity)));
                    }
                    if (type == Type_Homing) {
                        verticies[Index].color =
                            sf::Color(0, 0, 255, 128 * logisticCurve(*(Maturity)));
                    }
                    if (type == Type_NoFood) {
                        verticies[Index].color = sf::Color(250,0,0, 128*logisticCurve(*(Maturity)));
                    
                    }
                }
            } else {
                *Maturity = 0;
            }
        }
    }
    if (Rendering) {
        window->draw(verticies);
    }
    if (Diffusion) {
        diffuse_field(Field_Intensity);
    }
    return 1;
}
int view_field(struct ant_struct *Ant, unsigned short *Field, double *Angle) {
    // TODO: SIMD this
    unsigned short Max_Intensity = Trail_Maturity;
    int Detected = 0;
    double x1 = Ant->xpos;
    double y1 = Ant->ypos;
    double x2 = x1 + Ant_View_Range * cos(Ant->angle + Ant_View_Angle);
    double y2 = y1 + Ant_View_Range * sin(Ant->angle + Ant_View_Angle);
    double x3 = x1 + Ant_View_Range * cos(Ant->angle - Ant_View_Angle);
    double y3 = y1 + Ant_View_Range * sin(Ant->angle - Ant_View_Angle);

    // determine the rectangular bounding box of the triangle
    double min_x = x1;
    double max_x = x1;
    if (x2 < x1) {
        min_x = x2;
    } else {
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
    } else {
        max_y = y2;
    }
    if (y3 < y1) {
        min_y = y3;
    }
    if (y3 > y1) {
        max_y = y3;
    }

    // Loop through these coords
    for (int Col = (int)min_x; Col <= (int)max_x; Col++) {
        for (int Row = (int)min_y; Row <= (int)max_y; Row++) {
            if (is_point_in_triangle(Col, Row, (int)x1, (int)y1, (int)x2, (int)y2,
                        (int)x3, (int)y3)) {
                int Index = Canvas_X * Row + Col;
                if (Index > 0 && Index < Canvas_X * Canvas_Y) {
                    unsigned short Intensity = *(Field + Index);
                    if (Intensity > Max_Intensity) {
                        double DeltaX = (double)Col - Ant->xpos;
                        double DeltaY = (double)Row - Ant->ypos;
                        double MagSquared = pow(DeltaX, 2) + pow(DeltaY, 2);
                        if (MagSquared > 0.0) {
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

int scan_field_SIMD(struct ant_struct *Ant, unsigned short *Field, double *Angle) {
    unsigned short max_intensity = Trail_Maturity;
    int Detected = 0;
    int Best_Col = 0;
    int Best_Row = 0;
    int Row = -Ant_Sense_Range;
    for (Row -= Row*( ((int)Ant->ypos + Row) < 0); Row <= Ant_Sense_Range && Row + (int)Ant->ypos < Canvas_Y; Row++) { 
        int Col = -Ant_Sense_Range;
        for (Col -= Col* ( ((int)Ant->xpos + Col) < 0); Col <= Ant_Sense_Range && Col + (int)Ant->xpos < Canvas_X; Col++) {
            int Index = Canvas_X*(Row + (int)Ant->ypos) + (int)Ant->xpos + Col;
            int MagnitudeSquared = pow(Col, 2) + pow(Row, 2);
            unsigned short *intensity = (Field + Index);
            int Suitable = (MagnitudeSquared > 1 && *intensity > max_intensity && *intensity > Ant->max_intensity);

            Detected = Suitable || Detected;
            max_intensity = *intensity * Suitable + max_intensity * (!Suitable);
            Ant->max_intensity = *intensity * Suitable + (Ant->max_intensity) * (!Suitable);
            Best_Col = Suitable*Col + Best_Col*(!Suitable);
            Best_Row = Suitable*Row + Best_Row*(!Suitable);
        }
    }
    if (Detected) {
        *Angle = atan2( (float)Best_Row, (float)Best_Col); 
    }
    return Detected;
}

int scan_field(struct ant_struct *Ant, unsigned short *Field, double *Angle) {
    // return the angle of the most hormone concentration
    // Check a rectange around
    // TODO: SIMD this bitch
    unsigned short max_intensity = Trail_Maturity;
    int Detected = 0;
    for (int Row = -Ant_Sense_Range; Row <= Ant_Sense_Range; Row++) {
        for (int Col = -Ant_Sense_Range; Col <= Ant_Sense_Range; Col++) {
            int Index = Canvas_X * ((int)Ant->ypos + Row) + (int)Ant->xpos + Col;
            if (Index > 0 && Index < Canvas_X * Canvas_Y) {
                double DeltaX = (double)Col;
                double DeltaY = (double)Row;
                double MagnitudeSquared = pow(DeltaX, 2) + pow(DeltaY, 2);
                if (MagnitudeSquared > 1.0) {
                    unsigned short *intensity = (Field + Index);
                    if (*intensity > max_intensity && *intensity > Ant->max_intensity) {
                        *Angle = atan2(DeltaY, DeltaX);
                        Detected = 1;
                        max_intensity = *intensity;
                        Ant->max_intensity = *intensity;
                    }
                }
            }
        }
    }
    return Detected;
}

int Wipe_Field(struct ant_struct* Ant, unsigned short* Field_Maturity, double* Field_Intensity) {
    int Row = -Ant_Sense_Range;
    for (Row -= Row*( ((int)Ant->ypos + Row) < 0); Row <= Ant_Sense_Range && Row + (int)Ant->ypos < Canvas_Y; Row++) { 
        int Col = -Ant_Sense_Range;
        for (Col -= Col* ( ((int)Ant->xpos + Col) < 0); Col <= Ant_Sense_Range && Col + (int)Ant->xpos < Canvas_X; Col++) {
            int Index = Canvas_X*(Row + (int)Ant->ypos) + (int)Ant->xpos + Col;
            *(Field_Maturity + Index) = 0;
            *(Field_Intensity + Index) = 0;
        }
    }
}


void drop_trails(struct ant_struct *Ant, double *Field) {

    // Standard drop near ant
    int Index = Canvas_X * (int)Ant->ypos + (int)Ant->xpos;
    if (Index > 0 && Index < Canvas_X * Canvas_Y) {
        *(Field + Index) += Ant->timer;
    }

    /*
    //Doing this with a bunch of offests
    int OffsetRange = 3;
    for (int OffsetX = -OffsetRange; OffsetX <= OffsetRange; OffsetX++) {
    for (int OffsetY = -OffsetRange; OffsetY <= OffsetRange; OffsetY++) {
    int Index = Canvas_X*((int)Ant->ypos + OffsetY) + (int)Ant->xpos +
    OffsetY; if (Index > 0 && Index < Canvas_X*Canvas_Y) {
     *(Field + Index) += Ant->timer;
     }
     }
     }
     */
}


int ant_update(struct ant_struct *Ant, struct food_struct **Foods_List,
        double *Foraging_Field_Intensity,
        unsigned short *Foraging_Field_Maturity, double *Homing_Field_Intensity,
        unsigned short *Homing_Field_Maturity,
        struct colony_struct *Colonys_List,
        struct walls_struct *Walls_List) {
    Ant->max_intensity += Trail_Aging; 
    Ant->timer *= Trail_Decay;
    double Ant_Radius_from_center = sqrt(pow(Ant->xpos - Canvas_X/2, 2) + pow(Ant->ypos - Canvas_Y/2, 2));
    if ((*Ant).state == Ant_Foraging) {

        drop_trails(Ant, Homing_Field_Intensity);
        // check if food is left in the level
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
        while(Food_Item->next != NULL) {
            if(Food_Item->next->radius_from_center < Ant_Radius_from_center - Ant_View_Range) {
                Food_Item = Food_Item->next; 
            }
            else{
                break;
            }
        }
        struct food_struct* Start_Food = Food_Item;
        while (Looping) {

            // check if we can pick up the food
            if (in_interact_range(Ant, Food_Item, Type_Food)) {
                pickup_food(Ant, Food_Item);
                //deleting this could fuck the pointers
                if (Start_Food == Food_Item) {
                    Start_Food = delete_food(Food_Item, Foods_List);
                }
                else {
                    delete_food(Food_Item, Foods_List);
                }
                //If we can pickup the food check if there's other food nearby
                Food_Item = Start_Food;
                while(Looping) {

                    double Default  = 0;

                    if(in_sense_range(Ant, Food_Item, &Default, &Default, &Default, Type_Food)) {
                        Looping = 0;
                    }

                    if (Food_Item->next != NULL && Food_Item->radius_from_center < Ant_Radius_from_center + Ant_Sense_Range) {
                        Food_Item = Food_Item->next;
                    }
                    else {
                        Looping = 0;
                        Ant->state = Ant_Homing_NoFood;
                    }
                }
                double Angle = 0;
                if (scan_field_SIMD(Ant, Homing_Field_Maturity, &Angle)) {
                    Ant->angle = Angle;
                }
                move_randomly(Ant);
                return 1;

            }
            double Food_Rel_X = 0;
            double Food_Rel_Y = 0;
            double Distance = 0;
            if ((in_view_cone(Ant, Food_Item, &Food_Rel_X, &Food_Rel_Y, &Distance, Type_Food) || in_sense_range(Ant, Food_Item, &Food_Rel_X, &Food_Rel_Y, &Distance, Type_Food))) {
                if (Distance < Least_Mag_Squared || Least_Mag_Squared < 0) {
                    Least_Mag_Squared = Distance;
                    Move_X = Food_Rel_X;
                    Move_Y = Food_Rel_Y;
                }
            }
            if (Food_Item->next != NULL && Food_Item->radius_from_center < Ant_Radius_from_center + Ant_View_Range) {
                Food_Item = (Food_Item->next);
            } else {
                Looping = 0; // Terminate the loop
            }
        }
        if (Least_Mag_Squared > 0) {
            move_xy(Ant, Move_X, Move_Y);
            return 1;
        }

        double Angle = 0;
        if (view_field(Ant, Foraging_Field_Maturity, &Angle)) {
            Ant->angle = Angle;
        } else if (scan_field_SIMD(Ant, Foraging_Field_Maturity, &Angle)) {
            Ant->angle = Angle;
        }
        // Checking the colonies to reset timer
        Looping = 1;
        struct colony_struct *Colony = Colonys_List;
        double Placeholder = 0;
        while (Looping) {

            if (in_view_cone(Ant, Colony, &Placeholder, &Placeholder, &Placeholder,
                        Type_Colony)) {
                Ant->timer = Trail_Increment;
            }

            if (Colony->next == NULL) {
                Looping = 0;
            } else {
                Colony = Colony->next;
            }
        }

        move_randomly(Ant);
        return 1;
    }

    if ((*Ant).state == Ant_Homing || Ant->state == Ant_Homing_NoFood) {
        //TODO: Ant Homing needs to be memory based 
        if (Ant->state == Ant_Homing_NoFood) {
            Wipe_Field(Ant, Foraging_Field_Maturity, Foraging_Field_Intensity);
        }
        else {
            drop_trails(Ant, Foraging_Field_Intensity);
        }
        // Check if you can see a colony
        struct colony_struct *Colony = Colonys_List;
        double Homing_Angle;
        double Move_Angle;
        double Max_Squared_Distance = -1;
        int Looping = 1;
        int Found = 0;

        double Target_X;
        double Target_Y;
        double Distance;
        while (Looping) {

            if (in_interact_range(Ant, Colony, Type_Colony)) {

                Ant->state = Ant_Foraging;
                Ant->timer = Trail_Increment;
                Ant->max_intensity = 0.0;
                double Angle = 0;
                if (scan_field_SIMD(Ant, Foraging_Field_Maturity, &Angle)) {
                    Ant->angle = Angle;
                }
                move_randomly(Ant);
                return 1;
            }

            // then check if you can see some colonies
            if (in_view_cone(Ant, Colony, &Target_X, &Target_Y, &Distance,
                        Type_Colony)) {
                move_xy(Ant, Target_X, Target_Y);
            }

            if (Colony->next == NULL) {
                Looping = 0;
            } else {
                Colony = Colony->next;
            }
        }

        // could not find any colonies so now look for trails
        double Angle = 0;
        if (view_field(Ant, Homing_Field_Maturity, &Angle)) {
            Ant->angle = Angle;
        } else if (scan_field_SIMD(Ant, Homing_Field_Maturity, &Angle)) {
            Ant->angle = Angle;
        }

        // check if you can see some food
        if (*Foods_List != NULL) {
            Looping = 1;
            struct food_struct *Food_Item = *Foods_List;
            double Placeholder = 0;
            while(Food_Item->next != NULL) {
                if(Food_Item->next->radius_from_center < Ant_Radius_from_center - Ant_View_Range) {
                    Food_Item = Food_Item->next;
                }
                else {
                    break;
                }
            }
            while (Looping) {

                if (in_view_cone(Ant, Food_Item, &Placeholder, &Placeholder, &Placeholder, Type_Food)) {
                    Ant->timer = Trail_Increment;
                }

                if (Food_Item->next != NULL && Food_Item->radius_from_center < Ant_Radius_from_center + Ant_View_Range) {
                    Food_Item = (Food_Item->next); // Follow the trail
                } else {
                    Looping = 0; // Terminate the loop
                }
            }
        }

        move_randomly(Ant);
        return 1;
    }
    return -1;
}

struct setup_struct *setup() {

    struct setup_struct *Setup_Data = (struct setup_struct *)malloc(sizeof(struct setup_struct));

    Setup_Data->ants_list = NULL;
    for (int i = 0; i < Number_Ants; i++) {
        init_ant(&(Setup_Data->ants_list), Canvas_X / 2, Canvas_Y / 2);
    }

    Setup_Data->colonys_list = NULL;
    init_colony(&(Setup_Data->colonys_list), Canvas_X / 2, Canvas_Y / 2, Canvas_X * Canvas_Y / 60000);
    Setup_Data->foods_list = NULL;
    for (int i = 0; i < Number_Clusters; i++) {
        double Cluster_X = randfrom(3 * Ant_View_Range, Canvas_X - 3 * Ant_View_Range);
        double Cluster_Y = randfrom(3 * Ant_View_Range, Canvas_Y - 3 * Ant_View_Range);
        for (int i = 0; i < Number_Foods; i++) {
            init_food(&(Setup_Data->foods_list), Cluster_X + randfrom(-5, 5), Cluster_Y + randfrom(-5, 5));
        }
    }

    //Debugging food
    struct food_struct* Food = Setup_Data->foods_list;
    for(int i = 0; i<Number_Clusters*Number_Foods; i++) {
        Food = Food->next;
    }
    //Walls
    
    Setup_Data->walls_list = NULL;
    for(int i = 0; i < Number_Walls; i++) {
        init_wall(&Setup_Data->walls_list, Wall_Verticies);
    }
    return Setup_Data;
}

void loop(sf::RenderWindow *window, struct ant_struct **Ants_List,
        double *Foraging_Field_Intensity, unsigned short *Foraging_Field_Maturity,
        double *Homing_Field_Intensity, unsigned short *Homing_Field_Maturity,
        struct food_struct **Foods_List, struct colony_struct **Colonys_List, struct wall_struct** Walls_List,
        int Rendering) {

    if(Rendering) {
        struct wall_struct* Wall = *Walls_List;
        int Looping = 1;
        while(Looping) {
            render_wall(Wall, window);
            if(Wall->next == NULL) {
                Looping = 0;
            }
            else {
                Wall = Wall->next;
            }
        }

    }


    // ants
    struct ant_struct *Ant = *Ants_List;
    int Looping = 1;
    while (Looping) {
        ant_update(Ant, Foods_List, Foraging_Field_Intensity,
                Foraging_Field_Maturity, Homing_Field_Intensity,
                Homing_Field_Maturity, *Colonys_List, *Walls_List);
        if (Rendering) {
            render_ant(Ant, window);
        }
        if (Ant->next == NULL) {
            Looping = 0;
        } else {
            Ant = Ant->next;
        }
    }

    // foods
    if (Rendering) {
        struct food_struct *Food_Address = *Foods_List;
        Looping = 1;
        while (Looping) {
            render_food(Food_Address, window);
            if (Food_Address->next == NULL) {
                Looping = 0;
            } else {
                Food_Address = Food_Address->next;
            }
        }
        Looping = 1;
        struct colony_struct *Colony_Address = *Colonys_List;
        while (Looping) {
            render_colony(Colony_Address, window);
            if (Colony_Address->next == NULL) {
                Looping = 0;
            } else {
                Colony_Address = Colony_Address->next;
            }
        }
    }

    renderAndupdate_field(Foraging_Field_Intensity, Foraging_Field_Maturity,
            Type_Foraging, window, Rendering);
    renderAndupdate_field(Homing_Field_Intensity, Homing_Field_Maturity,
            Type_Homing, window, Rendering);

}

int main() {

    srand((unsigned int)time(NULL));
    struct setup_struct *Setup_Data = setup();

    // window init
    sf::RenderWindow window(sf::VideoMode(Canvas_X, Canvas_Y), "Ant Simulation");
    window.setVerticalSyncEnabled(
            true); // call it once, after creating the window
    window.setActive(true);
    window.setFramerateLimit(
            FrameRate); // could have somethign like microstepping in this

    // Elaborate setupdata
    struct ant_struct *Ants_List = (*Setup_Data).ants_list;
    struct food_struct *Foods_List = (*Setup_Data).foods_list;
    struct colony_struct *Colonys_List = (*Setup_Data).colonys_list;
    struct wall_struct *Walls_List = Setup_Data->walls_list;

    double *Foraging_Field_Intensity = (double *)malloc(Canvas_X * Canvas_Y * sizeof(double));
    double *Homing_Field_Intensity = (double *)malloc(Canvas_X * Canvas_Y * sizeof(double));
    unsigned short *Foraging_Field_Maturity = (unsigned short *)malloc(Canvas_X * Canvas_Y * sizeof(unsigned short));
    unsigned short *Homing_Field_Maturity = (unsigned short *)malloc(Canvas_X * Canvas_Y * sizeof(unsigned short));

    int i = 0;

    while (window.isOpen()) {
        sf::Event event;
        while (window.pollEvent(event)) {
            switch (event.type) {
                case sf::Event::Closed:
                    window.close();
                    // run the exit routine otherwise

                    break;
                default:
                    break;
            }
        }

        i++;
        int Rendering = (i % MicroStep == 0);
        if (Rendering) {
            window.clear(sf::Color::Black);
        }

        loop(&window, &Ants_List, Foraging_Field_Intensity, Foraging_Field_Maturity,
                Homing_Field_Intensity, Homing_Field_Maturity, &Foods_List,
                &Colonys_List, &Walls_List, Rendering);

        if (Rendering) {
            window.display();
        }
    }
    return 0;
}
