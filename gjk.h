// Original source code: https://github.com/kevinmoran/GJK/blob/master/GJK.h

#pragma once

#include <stdio.h>
#include <raymath.h>

//Kevin's implementation of the Gilbert-Johnson-Keerthi intersection algorithm
//and the Expanding Polytope Algorithm
//Most useful references (Huge thanks to all the authors):

// "Implementing GJK" by Casey Muratori:
// The best description of the algorithm from the ground up
// https://www.youtube.com/watch?v=Qupqu1xe7Io

// "Implementing a GJK Intersection Query" by Phill Djonov
// Interesting tips for implementing the algorithm
// http://vec3.ca/gjk/implementation/

// "GJK Algorithm 3D" by Sergiu Craitoiu
// Has nice diagrams to visualise the tetrahedral case
// http://in2gpu.com/2014/05/18/gjk-algorithm-3d/

// "GJK + Expanding Polytope Algorithm - Implementation and Visualization"
// Good breakdown of EPA with demo for visualisation
// https://www.youtube.com/watch?v=6rgiPrzqt9w
//-----------------------------------------------------------------------------

typedef struct GJKColliderMesh {
    float *vertices; // set of (X, Y, Z) coordinates
    size_t nVertices; // number of coordinates (vertices size = nVertices * 3)
    Vector3 pos;
} GJKColliderMesh;


//Returns true if two colliders are intersecting. Has optional Minimum Translation Vector output param;
//If supplied the EPA will be used to find the vector to separate coll1 from coll2
bool gjk(GJKColliderMesh *coll1, GJKColliderMesh *coll2, Vector3 *mtv);
//Internal functions used in the GJK algorithm
void update_simplex3(Vector3 *a, Vector3 *b, Vector3 *c, Vector3 *d, int *simp_dim, Vector3 *search_dir);
bool update_simplex4(Vector3 *a, Vector3 *b, Vector3 *c, Vector3 *d, int *simp_dim, Vector3 *search_dir);
//Expanding Polytope Algorithm. Used to find the mtv of two intersecting 
//colliders using the final simplex obtained with the GJK algorithm
Vector3 EPA(Vector3 a, Vector3 b, Vector3 c, Vector3 d, GJKColliderMesh* coll1, GJKColliderMesh* coll2);

#define GJK_MAX_NUM_ITERATIONS 64

Vector3 supportVec(GJKColliderMesh *mesh, Vector3 dir) {
    Vector3 vert, maxPoint;
    float maxDist = -100000.f, dist;
    for(int i = 0; i < mesh->nVertices * 3; i += 3) {
        vert = (Vector3){mesh->vertices[i], mesh->vertices[i + 1], mesh->vertices[i + 2]};
        //vert = Vector3Transform(vert, transform);
        dist = Vector3DotProduct(vert, dir);
        if(dist > maxDist)
            maxDist = dist, maxPoint = vert;
    }
    return maxPoint;
}

bool gjk(GJKColliderMesh *coll1, GJKColliderMesh *coll2, Vector3 *mtv){
    Vector3 a, b, c, d; //Simplex: just a set of points (a is always most recently added)
    Vector3 search_dir = Vector3Subtract(coll1->pos, coll2->pos); //initial search direction between colliders

    //Get initial point for simplex
    //c = coll2->support(search_dir) - coll1->support(-search_dir);
    c = Vector3Subtract(
        supportVec(coll2, search_dir),
        supportVec(coll1, Vector3Negate(search_dir))
    );
    search_dir = Vector3Negate(c); //search in direction of origin

    //Get second point for a line segment simplex
    //b = coll2->support(search_dir) - coll1->support(-search_dir);
    b = Vector3Subtract(
        supportVec(coll2, search_dir),
        supportVec(coll1, Vector3Negate(search_dir))
    );

    if(Vector3DotProduct(b, search_dir)<0) { return false; }//we didn't reach the origin, won't enclose it

    //search_dir = cross(cross(c-b,-b),c-b); //search perpendicular to line segment towards origin
    search_dir = Vector3CrossProduct(Vector3Subtract(c, b), Vector3Negate(b));
    search_dir = Vector3CrossProduct(search_dir, Vector3Subtract(c, b));
    if(search_dir.x == 0 && search_dir.y == 0 && search_dir.z == 0){ //origin is on this line segment
        //Apparently any normal search vector will do?
        //search_dir = cross(c-b, vec3(1,0,0)); //normal with x-axis
        search_dir = Vector3CrossProduct(
            Vector3Subtract(c, b), (Vector3){1, 0, 0}
        );
        //if(search_dir==vec3(0,0,0)) search_dir = cross(c-b, vec3(0,0,-1)); //normal with z-axis
        if(search_dir.x == 0 && search_dir.y == 0 && search_dir.z == 0) {
            search_dir = Vector3CrossProduct(
                Vector3Subtract(c, b), (Vector3){0, 0, -1}
            );
        }
    }
    int simp_dim = 2; //simplex dimension
    
    for(int iterations=0; iterations<GJK_MAX_NUM_ITERATIONS; iterations++)
    {
        //a = coll2->support(search_dir) - coll1->support(-search_dir);
        c = Vector3Subtract(
            supportVec(coll2, search_dir),
            supportVec(coll1, Vector3Negate(search_dir))
        );
        if(Vector3DotProduct(a, search_dir)<0) { return false; }//we didn't reach the origin, won't enclose it
    
        simp_dim++;
        if(simp_dim==3){
            update_simplex3(&a,&b,&c,&d,&simp_dim,&search_dir);
        }
        else if(update_simplex4(&a,&b,&c,&d,&simp_dim,&search_dir)) {
            if(mtv) *mtv = EPA(a,b,c,d,coll1,coll2);
            return true;
        }
    }//endfor
    return false;
}

//Triangle case
void update_simplex3(Vector3 *a, Vector3 *b, Vector3 *c, Vector3 *d, int *simp_dim, Vector3 *search_dir){
    /* Required winding order:
    //  b
    //  | \
    //  |   \
    //  |    a
    //  |   /
    //  | /
    //  c
    */
    //vec3 n = cross(b-a, c-a); //triangle's normal
    //vec3 AO = -a; //direction to origin
    Vector3 n = Vector3CrossProduct(
        Vector3Subtract(*b, *a),
        Vector3Subtract(*c, *a)
    );
    Vector3 AO = Vector3Negate(*a);

    //Determine which feature is closest to origin, make that the new simplex

    *simp_dim = 2;
    //float dp = dot(cross(b-a, n), AO);
    float dp = Vector3DotProduct(
        Vector3CrossProduct(Vector3Subtract(*b, *a), n),
        AO
    );
    if(dp>0){ //Closest to edge AB
        c = a;
        //simp_dim = 2;
        //search_dir = cross(cross(b-a, AO), b-a);
        *search_dir = Vector3CrossProduct(Vector3Subtract(*b, *a), AO);
        *search_dir = Vector3CrossProduct(*search_dir, Vector3Subtract(*b, *a));
        return;
    }
    //float dp = dot(cross(n, c-a), AO);
    float dp = Vector3DotProduct(
        Vector3CrossProduct(n, Vector3Subtract(*c, *a)),
        AO
    );
    if(dp>0){ //Closest to edge AC
        b = a;
        //simp_dim = 2;
        //search_dir = cross(cross(c-a, AO), c-a);
        *search_dir = Vector3CrossProduct(Vector3Subtract(*c, *a), AO);
        *search_dir = Vector3CrossProduct(*search_dir, Vector3Subtract(*c, *a));
        return;
    }
    
    *simp_dim = 3;
    if(Vector3DotProduct(n, AO)>0){ //Above triangle
        d = c;
        c = b;
        b = a;
        //simp_dim = 3;
        *search_dir = n;
        return;
    }
    //else //Below triangle
    d = b;
    b = a;
    //simp_dim = 3;
    *search_dir = Vector3Negate(n);
    return;
}

//Tetrahedral case
bool update_simplex4(Vector3 *a, Vector3 *b, Vector3 *c, Vector3 *d, int *simp_dim, Vector3 *search_dir){
    // a is peak/tip of pyramid, BCD is the base (counterclockwise winding order)
	//We know a priori that origin is above BCD and below a

    //Get normals of three new faces
    Vector3 ABC = Vector3CrossProduct(
        Vector3Subtract(*b, *a), Vector3Subtract(*c, *a)
    );
    Vector3 ACD = Vector3CrossProduct(
        Vector3Subtract(*c, *a), Vector3Subtract(*d, *a)
    );
    Vector3 ADB = Vector3CrossProduct(
        Vector3Subtract(*d, *a), Vector3Subtract(*b, *a)
    );

    Vector3 AO = Vector3Negate(*a); //dir to origin
    *simp_dim = 3; //hoisting this just cause

    //Plane-test origin with 3 faces
    /*
    // Note: Kind of primitive approach used here; If origin is in front of a face, just use it as the new simplex.
    // We just go through the faces sequentially and exit at the first one which satisfies dot product. Not sure this 
    // is optimal or if edges should be considered as possible simplices? Thinking this through in my head I feel like 
    // this method is good enough. Makes no difference for AABBS, should test with more complex colliders.
    */
    if(Vector3DotProduct(ABC, AO)>0){ //In front of ABC
    	d = c;
    	c = b;
    	b = a;
        *search_dir = ABC;
    	return false;
    }
    if(Vector3DotProduct(ACD, AO)>0){ //In front of ACD
    	b = a;
        *search_dir = ACD;
    	return false;
    }
    if(Vector3DotProduct(ADB, AO)>0){ //In front of ADB
    	c = d;
    	d = b;
    	b = a;
        *search_dir = ADB;
    	return false;
    }

    //else inside tetrahedron; enclosed!
    return true;

    //Note: in the case where two of the faces have similar normals,
    //The origin could conceivably be closest to an edge on the tetrahedron
    //Right now I don't think it'll make a difference to limit our new simplices
    //to just one of the faces, maybe test it later.
}

//Expanding Polytope Algorithm
//Find minimum translation vector to resolve collision
#define EPA_TOLERANCE 0.0001
#define EPA_MAX_NUM_FACES 64
#define EPA_MAX_NUM_LOOSE_EDGES 32
#define EPA_MAX_NUM_ITERATIONS 64
Vector3 EPA(Vector3 a, Vector3 b, Vector3 c, Vector3 d, GJKColliderMesh* coll1, GJKColliderMesh* coll2){
    Vector3 faces[EPA_MAX_NUM_FACES][4]; //Array of faces, each with 3 verts and a normal
    
    //Init with final simplex from GJK
    faces[0][0] = a;
    faces[0][1] = b;
    faces[0][2] = c;
    //faces[0][3] = normalise(cross(b-a, c-a)); //ABC
    faces[0][3] = Vector3Normalize(Vector3CrossProduct(
        Vector3Subtract(b, a), Vector3Subtract(c, a)
    ));
    faces[1][0] = a;
    faces[1][1] = c;
    faces[1][2] = d;
    //faces[1][3] = normalise(cross(c-a, d-a)); //ACD
    faces[1][3] = Vector3Normalize(Vector3CrossProduct(
        Vector3Subtract(c, a), Vector3Subtract(d, a)
    ));
    faces[2][0] = a;
    faces[2][1] = d;
    faces[2][2] = b;
    //faces[2][3] = normalise(cross(d-a, b-a)); //ADB
    faces[2][3] = Vector3Normalize(Vector3CrossProduct(
        Vector3Subtract(d, a), Vector3Subtract(b, a)
    ));
    faces[3][0] = b;
    faces[3][1] = d;
    faces[3][2] = c;
    //faces[3][3] = normalise(cross(d-b, c-b)); //BDC
    faces[0][3] = Vector3Normalize(Vector3CrossProduct(
        Vector3Subtract(d, b), Vector3Subtract(c, b)
    ));

    int num_faces=4;
    int closest_face;

    for(int iterations=0; iterations<EPA_MAX_NUM_ITERATIONS; iterations++){
        //Find face that's closest to origin
        float min_dist = Vector3DotProduct(faces[0][0], faces[0][3]);
        closest_face = 0;
        for(int i=1; i<num_faces; i++){
            float dist = Vector3DotProduct(faces[i][0], faces[i][3]);
            if(dist<min_dist){
                min_dist = dist;
                closest_face = i;
            }
        }

        //search normal to face that's closest to origin
        Vector3 search_dir = faces[closest_face][3]; 
        //Vector3 p = coll2->support(search_dir) - coll1->support(-search_dir);
        Vector3 p = Vector3Subtract(
            supportVec(coll2, search_dir),
            supportVec(coll1, Vector3Negate(search_dir))
        );

        if(Vector3DotProduct(p, search_dir)-min_dist<EPA_TOLERANCE){
            //Convergence (new point is not significantly further from origin)
            //return faces[closest_face][3]*dot(p, search_dir); //dot vertex with normal to resolve collision along normal!
            return Vector3Scale(faces[closest_face][3], Vector3DotProduct(p, search_dir));
        }

        Vector3 loose_edges[EPA_MAX_NUM_LOOSE_EDGES][2]; //keep track of edges we need to fix after removing faces
        int num_loose_edges = 0;

        //Find all triangles that are facing p
        for(int i=0; i<num_faces; i++)
        {
            //float dp = dot(faces[i][3], p-faces[i][0]);
            float dp = Vector3DotProduct(
                faces[i][3], Vector3Subtract(p, faces[i][0])
            );
            if(dp>0) //triangle i faces p, remove it
            {
                //Add removed triangle's edges to loose edge list.
                //If it's already there, remove it (both triangles it belonged to are gone)
                for(int j=0; j<3; j++) //Three edges per face
                {
                    Vector3 current_edge[2] = {faces[i][j], faces[i][(j+1)%3]};
                    int found_edge = 0;
                    for(int k=0; k<num_loose_edges; k++) //Check if current edge is already in list
                    {
                        //if(loose_edges[k][1]==current_edge[0] && loose_edges[k][0]==current_edge[1]){
                        if(Vector3Equals(loose_edges[k][1], current_edge[0]) &&
                           Vector3Equals(loose_edges[k][0], current_edge[1])
                        ) {
                            //Edge is already in the list, remove it
                            //THIS ASSUMES EDGE CAN ONLY BE SHARED BY 2 TRIANGLES (which should be true)
                            //THIS ALSO ASSUMES SHARED EDGE WILL BE REVERSED IN THE TRIANGLES (which 
                            //should be true provided every triangle is wound CCW)
                            loose_edges[k][0] = loose_edges[num_loose_edges-1][0]; //Overwrite current edge
                            loose_edges[k][1] = loose_edges[num_loose_edges-1][1]; //with last edge in list
                            num_loose_edges--;
                            found_edge = 1;
                            k=num_loose_edges; //exit loop because edge can only be shared once
                        }
                    }//endfor loose_edges

                    if(!found_edge){ //add current edge to list
                        // assert(num_loose_edges<EPA_MAX_NUM_LOOSE_EDGES);
                        if(num_loose_edges>=EPA_MAX_NUM_LOOSE_EDGES) break;
                        loose_edges[num_loose_edges][0] = current_edge[0];
                        loose_edges[num_loose_edges][1] = current_edge[1];
                        num_loose_edges++;
                    }
                }

                //Remove triangle i from list
                faces[i][0] = faces[num_faces-1][0];
                faces[i][1] = faces[num_faces-1][1];
                faces[i][2] = faces[num_faces-1][2];
                faces[i][3] = faces[num_faces-1][3];
                num_faces--;
                i--;
            }//endif p can see triangle i
        }//endfor num_faces
        
        //Reconstruct polytope with p added
        for(int i=0; i<num_loose_edges; i++)
        {
            // assert(num_faces<EPA_MAX_NUM_FACES);
            if(num_faces>=EPA_MAX_NUM_FACES) break;
            faces[num_faces][0] = loose_edges[i][0];
            faces[num_faces][1] = loose_edges[i][1];
            faces[num_faces][2] = p;
            //faces[num_faces][3] = normalise(cross(loose_edges[i][0]-loose_edges[i][1], loose_edges[i][0]-p));
            faces[num_faces][3] = Vector3Normalize(Vector3CrossProduct(
                Vector3Subtract(loose_edges[i][0], loose_edges[i][1]),
                Vector3Subtract(loose_edges[i][0], p)
            ));

            //Check for wrong normal to maintain CCW winding
            float bias = 0.000001; //in case dot result is only slightly < 0 (because origin is on face)
            if(Vector3DotProduct(faces[num_faces][0], faces[num_faces][3])+bias < 0){
                Vector3 temp = faces[num_faces][0];
                faces[num_faces][0] = faces[num_faces][1];
                faces[num_faces][1] = temp;
                faces[num_faces][3] = Vector3Negate(faces[num_faces][3]);
            }
            num_faces++;
        }
    } //End for iterations
    printf("EPA did not converge\n");
    //Return most recent closest point
    return Vector3Scale(
        faces[closest_face][3],
        Vector3DotProduct(faces[closest_face][0], faces[closest_face][3])
    );
}