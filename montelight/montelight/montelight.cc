// ==Montelight==
// Tegan Brennan, Stephen Merity, Taiyo Wilson
#include <cmath>
#include <string>
#include <iomanip>
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <filesystem>

#define EPSILON 0.001f

using namespace std;

// Globals
bool EMITTER_SAMPLING = true;

struct Vector {
  double x, y, z;
  //
  Vector(const Vector &o) : x(o.x), y(o.y), z(o.z) {}
  Vector(double x_=0, double y_=0, double z_=0) : x(x_), y(y_), z(z_) {}
  inline Vector operator+(const Vector &o) const {
    return Vector(x + o.x, y + o.y, z + o.z);
  }
  inline Vector &operator+=(const Vector &rhs) {
    x += rhs.x; y += rhs.y; z += rhs.z;
    return *this;
  }
  inline Vector operator-(const Vector &o) const {
    return Vector(x - o.x, y - o.y, z - o.z);
  }
  inline Vector operator*(const Vector &o) const {
    return Vector(x * o.x, y * o.y, z * o.z);
  }
  inline Vector operator/(double o) const {
    return Vector(x / o, y / o, z / o);
  }
  inline Vector operator*(double o) const {
    return Vector(x * o, y * o, z * o);
  }
  inline double dot(const Vector &o) const {
    return x * o.x + y * o.y + z * o.z;
  }
  inline Vector &norm(){
    return *this = *this * (1 / sqrt(x * x + y * y + z * z));
  }
  inline Vector cross(Vector &o){
    return Vector(y * o.z - z * o.y, z * o.x - x * o.z, x * o.y - y * o.x);
  }
  inline double min() {
    return fmin(x, fmin(y, z));
  }
  inline double max() {
    return fmax(x, fmax(y, z));
  }
  inline Vector &abs() {
    x = fabs(x); y = fabs(y); z = fabs(z);
    return *this;
  }
  inline Vector &clamp() {
    // C++11 lambda function: http://en.cppreference.com/w/cpp/language/lambda
    auto clampDouble = [](double x) {
      if (x < 0) return 0.0;
      if (x > 1) return 1.0;
      return x;
    };
    x = clampDouble(x); y = clampDouble(y); z = clampDouble(z);
    return *this;
  }
};

struct Ray {
  Vector origin, direction;
  Ray(const Vector &o_, const Vector &d_) : origin(o_), direction(d_) {}
};

struct Image {
  unsigned int width, height;
  Vector *pixels, *current;
  unsigned int *samples;
  std::vector<Vector> *raw_samples;
  //
  Image(unsigned int w, unsigned int h) : width(w), height(h) {
    pixels = new Vector[width * height];
    // Pixel sample counts must start at zero before setPixel increments them.
    samples = new unsigned int[width * height]();
    current = new Vector[width * height];
    //raw_samples = new std::vector<Vector>[width * height];
  }
  Vector getPixel(unsigned int x, unsigned int y) {
    unsigned int index = (height - y - 1) * width + x;
    return current[index];
  }
  void setPixel(unsigned int x, unsigned int y, const Vector &v) {
    unsigned int index = (height - y - 1) * width + x;
    pixels[index] += v;
    samples[index] += 1;
    current[index] = pixels[index] / samples[index];
    //raw_samples[index].push_back(v);
  }
  Vector getSurroundingAverage(int x, int y, int pattern=0) {
    unsigned int index = (height - y - 1) * width + x;
    Vector avg;
    int total;
    for (int dy = -1; dy < 2; ++dy) {
      for (int dx = -1; dx < 2; ++dx) {
        if (pattern == 0 && (dx != 0 && dy != 0)) continue;
        if (pattern == 1 && (dx == 0 || dy == 0)) continue;
        if (dx == 0 && dy == 0) {
          continue;
        }
        if (x + dx < 0 || x + dx > width - 1) continue;
        if (y + dy < 0 || y + dy > height - 1) continue;
        index = (height - (y + dy) - 1) * width + (x + dx);
        avg += current[index];
        total += 1;
      }
    }
    return avg / total;
  }
  inline double toInt(double x) {
    return pow(x, 1 / 2.2f) * 255;
  }
  void save(std::string filePrefix) {
    std::string filename = filePrefix + ".ppm";
    std::ofstream f;
    f.open(filename.c_str(), std::ofstream::out);
    // PPM header: P3 => RGB, width, height, and max RGB value
    f << "P3 " << width << " " << height << " " << 255 << std::endl;
    // For each pixel, write the space separated RGB values
    for (int i=0; i < width * height; i++) {
      auto p = pixels[i] / samples[i];
      unsigned int r = fmin(255, toInt(p.x)), g = fmin(255, toInt(p.y)), b = fmin(255, toInt(p.z));
      f << r << " " << g << " " << b << std::endl;
    }
  }
  void saveHistogram(std::string filePrefix, int maxIters) {
    std::string filename = filePrefix + ".ppm";
    std::ofstream f;
    f.open(filename.c_str(), std::ofstream::out);
    // PPM header: P3 => RGB, width, height, and max RGB value
    f << "P3 " << width << " " << height << " " << 255 << std::endl;
    // For each pixel, write the space separated RGB values
    for (int i=0; i < width * height; i++) {
      auto p = samples[i] / maxIters;
      unsigned int r, g, b;
      r= g = b = fmin(255, 255 * p);
      f << r << " " << g << " " << b << std::endl;
    }
  }
  ~Image() {
    delete[] pixels;
    delete[] samples;
  }
};

enum Material {
  DIFFUSE,
  GLASS,
  MIRROR
};

struct Shape {
  Vector color, emit;
  Material material;
  //
  Shape(const Vector color_, const Vector emit_, Material material_ = DIFFUSE) :
    color(color_), emit(emit_), material(material_) {}
  virtual double intersects(const Ray &r) const { return 0; }
  virtual Vector randomPoint() const { return Vector(); }
  virtual Vector getNormal(const Vector &p) const { return Vector(); }
};

struct Sphere : Shape {
  Vector center;
  double radius;
  //
  Sphere(const Vector center_, double radius_, const Vector color_, const Vector emit_,
         Material material_ = DIFFUSE) :
    Shape(color_, emit_, material_), center(center_), radius(radius_) {}
  double intersects(const Ray &r) const {
    // Find if, and at what distance, the ray intersects with this object
    // Equation follows from solving quadratic equation of (r - c) ^ 2
    // http://wiki.cgsociety.org/index.php/Ray_Sphere_Intersection
    Vector offset = r.origin - center;
    double a = r.direction.dot(r.direction);
    double b = 2 * offset.dot(r.direction);
    double c = offset.dot(offset) - radius * radius;
    // Find discriminant for use in quadratic equation (b^2 - 4ac)
    double disc = b * b - 4 * a * c;
    // If the discriminant is negative, there are no real roots
    // (ray misses sphere)
    if (disc < 0) {
      return 0;
    }
    // The smallest positive root is the closest intersection point
    disc = sqrt(disc);
    double t = - b - disc;
    if (t > EPSILON) {
      return t / 2;
    }
    t = - b + disc;
    if (t > EPSILON) {
      return t / 2;
    }
    return 0;
  }
  Vector randomPoint() const {
    // TODO: Improved methods of random point generation as this is not 100% even
    // See: https://www.jasondavies.com/maps/random-points/
    //
    // Get random spherical coordinates on light
    double theta = drand48() * M_PI;
    double phi = drand48() * 2 * M_PI;
    // Convert to Cartesian and scale by radius
    double dxr = radius * sin(theta) * cos(phi);
    double dyr = radius * sin(theta) * sin(phi);
    double dzr = radius * cos(theta);
    return Vector(center.x + dxr, center.y + dyr, center.z + dzr);
  }
  Vector getNormal(const Vector &p) const {
    // Point must have collided with surface of sphere which is at radius
    // Normalize the normal by using radius instead of a sqrt call
    return (p - center) / radius;
  }
};

// Set up our testing scenes
// They're all Cornell box inspired: http://graphics.ucsd.edu/~henrik/images/cbox.html
/////////////////////////
// Scene format: Sphere(position, radius, color, emission)
/////////////////////////
std::vector<Shape *> simpleScene = {
  // Left sphere
  new Sphere(Vector(1e5+1,40.8,81.6), 1e5f, Vector(.75,.25,.25), Vector()),
  // Right sphere
  new Sphere(Vector(-1e5+99,40.8,81.6), 1e5f, Vector(.25,.25,.75), Vector()),
  // Back sphere
  new Sphere(Vector(50,40.8, 1e5), 1e5f, Vector(.75,.75,.75), Vector()),
  // Floor sphere
  new Sphere(Vector(50, 1e5, 81.6), 1e5f, Vector(.75,.75,.75), Vector()),
  // Roof sphere
  new Sphere(Vector(50,-1e5+81.6,81.6), 1e5f, Vector(.75,.75,.75), Vector()),
  // Traditional mirror sphere
  new Sphere(Vector(27,16.5,47), 16.5f, Vector(1,1,1) * 0.799, Vector()),
  // Traditional glass sphere
  new Sphere(Vector(73,16.5,78), 16.5f, Vector(1,1,1) * 0.799, Vector()),
  // Light source
  //new Sphere(Vector(50,681.6-.27,81.6), 600, Vector(1,1,1) * 0.5, Vector(12,12,12))
  new Sphere(Vector(50,65.1,81.6), 8.5, Vector(), Vector(4,4,4) * 100) // Small = 1.5, Large = 8.5
};
/////////////////////////
std::vector<Shape *> complexScene = {
  new Sphere(Vector(1e5+1,40.8,81.6), 1e5f, Vector(.75,.25,.25), Vector()), // Left
  new Sphere(Vector(-1e5+99,40.8,81.6), 1e5f, Vector(.25,.25,.75), Vector()), // Right
  new Sphere(Vector(50,40.8, 1e5), 1e5f, Vector(.75,.75,.75), Vector()), // Back
  new Sphere(Vector(50, 1e5, 81.6), 1e5f, Vector(.75,.75,.75), Vector()), //Bottom
  new Sphere(Vector(50,-1e5+81.6,81.6), 1e5f, Vector(.75,.75,.75), Vector()), // Top
    
  // Other material spheres are disabled to isolate the glass sphere while debugging.
  new Sphere(Vector(20,16.5,40), 16.5f, Vector(1,1,1) * 0.799, Vector(), DIFFUSE), // Left: diffuse
  new Sphere(Vector(50,33,80), 16.5f, Vector(1,1,1) * 0.9, Vector(), GLASS), // Glass
  new Sphere(Vector(75,16.5,100), 16.5f, Vector(1,1,1) * 0.799, Vector(), MIRROR), // Right: mirror
  //                pos(lr-updown-inout)
    
  // new Sphere(Vector(50,65.1,40), 1.5, Vector(), Vector(4,4,4) * 100), // Light
  new Sphere(Vector(50,65,120), 5.f, Vector(), Vector(4,4,4) * 200), // Light
};
//

struct Tracer {
  std::vector<Shape *> scene;
  //
  Tracer(const std::vector<Shape *> &scene_) : scene(scene_) {}
  std::pair<Shape *, double> getIntersection(const Ray &r) const {
    Shape *hitObj = NULL;
    double closest = 1e20f;
    for (Shape *obj : scene) {
      double distToHit = obj->intersects(r);
      if (distToHit > 0 && distToHit < closest) {
        hitObj = obj;
        closest = distToHit;
      }
    }
    return std::make_pair(hitObj, closest);
  }

  
  // Get the radiance of a ray, recursively
  // This is the main function that implements the path tracing algorithm
  // It recursively traces rays and accumulates the radiance from the scene
  // The depth parameter is used to limit the number of recursive calls
  // to prevent infinite recursion and to implement Russian Roulette sampling
  Vector getRadiance(const Ray &r, int depth) {
    // Work out what (if anything) was hit
    auto result = getIntersection(r);
    Shape *hitObj = result.first;
    // If no intersection, return black
    if (!hitObj) {
      return Vector();
    }
    // Russian roulette: terminate some long paths according to the material reflectance.
    double rouletteWeight = 1.0;
    if (depth > 4) {
      const double survivalProbability = hitObj->color.max();
      if (depth > 20 || survivalProbability <= 0 || drand48() > survivalProbability) {
        return Vector();
      }
      // Compensate surviving paths for the chance that an equivalent path was terminated.
      rouletteWeight = 1.0 / survivalProbability;
    }
    Vector hitPos = r.origin + r.direction * result.second;
    Vector norm = hitObj->getNormal(hitPos);
    // Orient the normal according to how the ray struck the object
    if (norm.dot(r.direction) > 0) {
      norm = norm * -1;
    }

    // Ideal specular reflection for mirrors.
    if (hitObj->material == MIRROR) {
      Vector reflectedDirection = r.direction - norm * (2 * r.direction.dot(norm));
      Vector reflected = getRadiance(Ray(hitPos, reflectedDirection.norm()), depth + 1);
      return (hitObj->emit + hitObj->color * reflected) * rouletteWeight;
    }

    // Dielectric refraction with Fresnel reflection and total internal reflection.
    if (hitObj->material == GLASS) {
      const double refractiveIndex = 1.5;
      const double nc = 1.0;
      const double nt = refractiveIndex;
      const bool entering = hitObj->getNormal(hitPos).dot(r.direction) < 0;
      const double nnt = entering ? nc / nt : nt / nc;
      const double ddn = r.direction.dot(norm);
      const double cos2t = 1 - nnt * nnt * (1 - ddn * ddn);
      Vector reflectedDirection = r.direction - norm * (2 * ddn);
      Vector reflected = getRadiance(Ray(hitPos, reflectedDirection.norm()), depth + 1);
      if (cos2t < 0) {
        return (hitObj->emit + hitObj->color * reflected) * rouletteWeight;
      }

      const double sign = entering ? 1 : -1;
      Vector transmittedDirection = (r.direction * nnt -
        hitObj->getNormal(hitPos) * (sign * (ddn * nnt + sqrt(cos2t)))).norm();
      const double r0 = (nt - nc) / (nt + nc);
      const double r0Squared = r0 * r0;
      // On exit, use the outward surface normal so the transmitted cosine stays positive.
      const double c = 1 - (entering ? -ddn : transmittedDirection.dot(hitObj->getNormal(hitPos)));
      const double reflectance = r0Squared + (1 - r0Squared) * c * c * c * c * c;
      Vector transmitted = getRadiance(Ray(hitPos, transmittedDirection), depth + 1);
      return (hitObj->emit + hitObj->color *
        (reflected * reflectance + transmitted * (1 - reflectance))) * rouletteWeight;
    }

    // Work out the contribution from directly sampling the emitters
    // This is the "direct lighting" part of the algorithm
    Vector lightSampling; // Accumulator for light sampling
    // If we are not sampling emitters, we skip this part
    if (EMITTER_SAMPLING) {
      for (Shape *light : scene) {
        // Skip any objects that don't emit light
        if (light->emit.max() == 0) {
          continue;
        }
        Vector lightPos = light->randomPoint(); // Get a random point on the light source
        Vector lightDirection = (lightPos - hitPos).norm(); // Direction to light source
        Ray rayToLight = Ray(hitPos, lightDirection); // Create a ray to the light source
        // Check if the ray to the light source intersects with any other objects
        auto lightHit = getIntersection(rayToLight); 
        //
        if (light == lightHit.first) {
          // If the ray to the light source does not hit any other objects, we can sample the light
          // Calculate the cosine of the angle between the light direction and the normal at the hit position
          // The dot product gives us the cosine of the angle between the two vectors
          // If the cosine is positive, the light is hitting the surface
          // If the cosine is negative, the light is hitting the surface from the other side
          // If the cosine is zero, the light is hitting the surface at a right angle
          // We only want to sample the light if the cosine is positive, as this means the light is hitting the surface
          double wi = lightDirection.dot(norm);
          
          if (wi > 0) {
            
            double srad = 1.5; // Radius of the light source
            //double srad = 600;

            // Calculate the maximum angle between the light direction and the normal at the hit position
            // This is done using the radius of the light source and the distance to the light source
            // cos(a_max) = sqrt(1 - (r^2 / d^2))
            // This gives us the cosine of the maximum angle between the light direction and the normal at the hit position
            // We use this to calculate the solid angle of the light source as seen from the hit position
            double cos_a_max = sqrt(1-srad*srad/(hitPos - lightPos).dot(hitPos - lightPos)); 

            // Calculate the solid angle of the light source as seen from the hit position
            // This is the area of the light source divided by the square of the distance to the light source
            // The area of the light source is 2 * pi * (1 - cos_a_max)            
            double omega = 2*M_PI*(1-cos_a_max);

            // The solid angle is then multiplied by the emission of the light source
            // and the cosine of the angle between the light direction and the normal at the hit position
            // Finally, we multiply by M_1_PI to normalize the result. Note: M_1_PI is a constant defined in <cmath> that is equal to 1 / pi
            // This is used to normalize the solid angle so that it is between 0 and 1
            // The final result is the contribution of the light source to the radiance at the hit position
            lightSampling += light->emit * wi * omega * M_1_PI;
          }
        }
      }
    }
    // ^ This implements what's known as "Next Event Estimation" - 
    // explicitly sampling light sources to improve the efficiency of the path tracer. 
    // This is a form of "direct lighting" where we sample the light sources directly
    // instead of relying on the Monte Carlo method to sample them indirectly.

    
    // Work out contribution from reflected light
    // Diffuse reflection condition:
    // Create orthogonal coordinate system defined by (x=u, y=v, z=norm)
    double angle = 2 * M_PI * drand48();
    double dist_cen = sqrt(drand48());
    Vector u;
    if (fabs(norm.x) > 0.1) {
      u = Vector(0, 1, 0);
    } else {
      u = Vector(1, 0, 0);
    }
    u = u.cross(norm).norm();
    Vector v = norm.cross(u);
    // Direction of reflection
    Vector d = (u * cos(angle) * dist_cen + v * sin(angle) * dist_cen + norm * sqrt(1 - dist_cen * dist_cen)).norm();

    // Recurse
    Vector reflected = getRadiance(Ray(hitPos, d), depth + 1);
    //
    if (!EMITTER_SAMPLING || depth == 0) {
      return (hitObj->emit + hitObj->color * lightSampling + hitObj->color * reflected) * rouletteWeight;
    }
    return (hitObj->color * lightSampling + hitObj->color * reflected) * rouletteWeight;
  }
};

int main(int argc, const char *argv[]) {
  /////////////////////////
  // Variables to modify the process or the images
  EMITTER_SAMPLING = true;
  int w = 256, h = 256;
  int SNAPSHOT_INTERVAL = 100;
    unsigned int SAMPLES = 2;//50;
  bool FOCUS_EFFECT = false;
  double FOCAL_LENGTH = 35;
  double APERTURE_FACTOR = 1; // ratio of original/new aperture (>1: smaller view angle, <1: larger view angle)
  // Initialize the image
  Image img(w, h);
  /////////////////////////
  // Set which scene should be raytraced
  auto &scene = complexScene;
  Tracer tracer = Tracer(scene);
  // Create the snapshot directory so intermediate saves work on a fresh run.
  std::filesystem::create_directories("temp");
  /////////////////////////
  // Set up the camera
  double aperture = 0.5135 / APERTURE_FACTOR;
  Vector cx = Vector((w * aperture) / h, 0, 0);
  Vector dir_norm = Vector(0, -0.042612, -1).norm();
  double L = 140;
  double L_new = APERTURE_FACTOR * L;
  double L_diff = L - L_new;
  Vector cam_shift = dir_norm * (L_diff);
  if (L_diff < 0){
    cam_shift = cam_shift * 1.5;
  }
  L = L_new;
  Ray camera = Ray(Vector(50, 52, 295.6) + cam_shift, dir_norm);
  // Cross product gets the vector perpendicular to cx and the "gaze" direction
  Vector cy = (cx.cross(camera.direction)).norm() * aperture;
  /////////////////////////
  // Take a set number of samples per pixel
  for (int sample = 0; sample < SAMPLES; ++sample) {
    std::cout << "Taking sample " << sample << "\r" << std::flush;
    if (sample && sample % SNAPSHOT_INTERVAL == 0) {
      std::ostringstream fn;
      fn << std::setfill('0') << std::setw(5) << sample;
      img.save("temp/render_" + fn.str());
    }
    // For each pixel, sample a ray in that direction
    for (int y = 0; y < h; ++y) {
      for (int x = 0; x < w; ++x) {
        /*
        Vector target = img.getPixel(x, y);
        double A = (target - img.getSurroundingAverage(x, y, sample % 2)).abs().max() / (100 / 255.0);
        if (sample > 10 && drand48() > A) {
          continue;
        }
        ++updated;
        */
        // Jitter pixel randomly in dx and dy according to the tent filter
        double Ux = 2 * drand48();
        double Uy = 2 * drand48();
        double dx;
        if (Ux < 1) {
          dx = sqrt(Ux) - 1;
        } else {
          dx = 1 - sqrt(2 - Ux);
        }
        double dy;
        if (Uy < 1) {
          dy = sqrt(Uy) - 1;
        } else {
          dy = 1 - sqrt(2 - Uy);
        }
        // Calculate the direction of the camera ray
        Vector d = (cx * (((x+dx) / float(w)) - 0.5)) + (cy * (((y+dy) / float(h)) - 0.5)) + camera.direction;
        Ray ray = Ray(camera.origin + d * 140, d.norm());
        // If we're actually using depth of field, we need to modify the camera ray to account for that
        if (FOCUS_EFFECT) {
          // Calculate the focal point
          Vector fp = (camera.origin + d * L) + d.norm() * FOCAL_LENGTH;
          // Get a pixel point and new ray direction to calculate where the rays should intersect
          Vector del_x = (cx * dx * L / float(w));
          Vector del_y = (cy * dy * L / float(h));
          Vector point = camera.origin + d * L;
          point = point + del_x + del_y;
          d = (fp - point).norm();
          ray = Ray(camera.origin + d * L, d.norm());
        }
        // Retrieve the radiance of the given hit location (i.e. brightness of the pixel)
        Vector rads = tracer.getRadiance(ray, 0);
        // Clamp the radiance so it is between 0 and 1
        // If we don't do this, antialiasing doesn't work properly on bright lights
        rads.clamp();
        // Add result of sample to image
        img.setPixel(x, y, rads);
      }
    }
  }
  // Save the resulting raytraced image
  img.save("render");
  return 0;
}
