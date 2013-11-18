#include "debug.h"

/* Some constant shits */
static const int    g_numShells = 24;
static       double g_furHeight = 0.21;
static       double g_hairyness = 0.7;
static const Cvec3  g_gravity(0, -0.5, 0);
static       double g_timeStep = 0.02;
static       double g_numStepsPerFrame = 10;
static       double g_damping = 0.96;
static       double g_stiffness = 4;

/** Bunny node */ // useful comment
static shared_ptr<SgRbtNode> g_bunnyNode;

/** The bunny mesh */
static Mesh g_bunnyMesh;
/** Shell geometries */
static vector<shared_ptr<SimpleGeometryPNX> > g_bunnyShellGeometries;

/** Used for physical simulation */
static int g_simulationsPerSecond = 60;

/** The hair tip position in world-space coordinates */
static std::vector<Cvec3> g_tipPos;
/** The hair tip velocity in world-space coordinates */
static std::vector<Cvec3> g_tipVelocity;

/**
 * Given a vertex on the bunny, returns the at-rest position of the hair tip.
 * @param  v The vertex on the bunny.
 * @return   The position of the tip of the hair.
 */
Cvec3 getAtRestTipPosition(Mesh::Vertex v) {
  return v.getNormal() * g_furHeight;
}

/**
 * Sets the tip positions to be the tips of the bunny's hair positions. Sets the
 * velocities to be initialized to zero.
 * @param mesh The bunny mesh.
 */
void initializeBunnyPhysics(Mesh &mesh) {
  for (int i = 0; i < mesh.getNumVertices(); i++) {
    g_tipPos.push_back(getAtRestTipPosition(mesh.getVertex(i)));
    g_tipVelocity.push_back(Cvec3());
  }
}

/**
 * Computes the vertex on a bunny shell.
 *
 * For each vertex with position p, compute the longest hair position s
 * Compute n = (s - p) / g_numShells
 * Compute our specific vertex position with p + n * layer
 *
 * @param          v The vertex on the bunny itself.
 * @param          i The layer of the bunny that we're computing.
 * @param textureVec The vector corresponding to the texture location we should
 *                   map to.
 */
static VertexPNX computeHairVertex(
    Mesh::Vertex v,
    int i,
    Cvec2 textureVec,
    RigTForm bunnyRbt,
    RigTForm invBunnyRbt) {

  // const Cvec3 t = g_tipPos[v.getIndex()];
  // const Cvec3 t = convertFrame(invBunnyRbt, getAtRestTipPosition(v));
  
  const Cvec3 p = v.getPosition();
  // const Cvec3 p = convertFrame(invBunnyRbt, v.getPosition());
  
  const Cvec3 n = v.getNormal() * (g_furHeight / g_numShells);
  // const Cvec3 n = convertFrame(invBunnyRbt, v.getNormal() * (g_furHeight / g_numShells));

  // const Cvec3 s = n * g_furHeight;
  const Cvec3 s = getAtRestTipPosition(v);
  const Cvec3 t = s;
  
  // const Cvec3 d = (t - p - n) / (g_numShells - 1);
  const Cvec3 d = ((t - p - n * g_numShells - n) / (g_numShells * g_numShells + g_numShells)) * 2;
  // const Cvec3 d = convertFrame(invBunnyRbt, (t - p - n) / (g_numShells - 1));

  // v.getPosition() + (bunnyRbt * g_tipPos[v.getIndex()] / g_numShells) * i,
  return VertexPNX(
    // v.getPosition() + (getAtRestTipPosition(v) / g_numShells) * i,
    v.getPosition() + d * i,
    v.getNormal(),
    textureVec
  );
}



/**
 * Returns the vertices for the layer-th layer of the bunny shell.
 */
static vector<VertexPNX> getBunnyShellGeometryVertices(
    Mesh &mesh,
    int layer,
    RigTForm bunnyRbt,
    RigTForm invBunnyRbt) {
  vector<VertexPNX> vs;
  /* For each face: */
  for (int i = 0; i < mesh.getNumFaces(); i++) {
    Mesh::Face f = mesh.getFace(i);
    /* For each vertex of each face: */
    for (int j = 1; j < f.getNumVertices() - 1; j++) {
      vs.push_back(computeHairVertex(f.getVertex(  0), layer, Cvec2(0, 0)          , bunnyRbt, invBunnyRbt));
      vs.push_back(computeHairVertex(f.getVertex(  j), layer, Cvec2(g_hairyness, 0), bunnyRbt, invBunnyRbt));
      vs.push_back(computeHairVertex(f.getVertex(j+1), layer, Cvec2(0, g_hairyness), bunnyRbt, invBunnyRbt));
    }
  }

  return vs;
}

static void updateHairCalculation(
    Mesh::Vertex vec,
    int vertexIndex,
    RigTForm bunnyRbt,
    RigTForm invBunnyRbt) {
  /* Reassignments so that we're consistent with notation in the assignment. */
  double T = g_timeStep;
  Cvec3 p = invBunnyRbt * vec.getPosition();
  Cvec3 s = invBunnyRbt * getAtRestTipPosition(vec);
  Cvec3 t = g_tipPos[vertexIndex];
  Cvec3 v = g_tipVelocity[vertexIndex];

  /* Step 1: Compute f */
  Cvec3 f = g_gravity + (s - t) * g_stiffness;
  /* Step 2: Update t */
  t = t + v * T;
  /* Step 3: Constrain t */
  g_tipPos[vertexIndex] = (p + (t - p) / norm(t - p) * g_furHeight);
  /* Step 4: Update v */
  g_tipVelocity[vertexIndex] = ((v + f * T) * g_damping);
}

/**
 * Updates the hair calculations for the bunny based on the physics simulation
 * descriptions provided in the assignment.
 */
static void updateHairs(Mesh &mesh) {
  cout << "Update heirs" << endl;
  RigTForm bunnyRbt = getPathAccumRbt(g_world, g_bunnyNode);
  RigTForm invBunnyRbt = inv(bunnyRbt);
  for (int i = 0; i < mesh.getNumVertices(); i++) {
    Mesh::Vertex v = mesh.getVertex(i);
    updateHairCalculation(v, i, bunnyRbt, invBunnyRbt);
  }
}

/**
 * Performs dynamics simulation g_simulationsPerSecond times per second
 */
static void hairsSimulationCallback(int _) {
  /* Update the hair dynamics. HACK: Ideally, we'd be passing in g_bunnyMesh to
     this function, but that's hard since it's a fucking callback. */
  updateHairs(g_bunnyMesh);
  /* Schedule this to get called again */
  glutTimerFunc(1250 / g_simulationsPerSecond, hairsSimulationCallback, _);
}

static void prepareBunnyForRendering() {
  printVector("First g_tipPos: ", g_tipPos[0]);
  // printVector("First g_tipVelocity: ", g_tipVelocity[0]);
  RigTForm bunnyRbt = getPathAccumRbt(g_world, g_bunnyNode);
  RigTForm invBunnyRbt = inv(bunnyRbt);
  for (int i = 0; i < g_numShells; ++i) {
    vector<VertexPNX> verticies =
      getBunnyShellGeometryVertices(g_bunnyMesh, i, bunnyRbt, invBunnyRbt);
    g_bunnyShellGeometries[i]->upload(&verticies[0], verticies.size());
  }
}
