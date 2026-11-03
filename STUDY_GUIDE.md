# Montelight Code Study Guide

This guide follows the Xcode project source at `montelight/montelight/montelight.cc`. The repository also has a separate root-level `montelight.cc`; keep that distinction in mind when building or comparing behavior.

## What the program does

Montelight is a small Monte Carlo path tracer. It renders a Cornell box style scene made from spheres, traces camera rays into that scene, estimates incoming light at each hit, and averages multiple noisy samples into a PPM image.

## Render flow

1. `main()` sets render options and selects `complexScene`.
2. For every pixel and each sample pass, it jitters the ray within the pixel using a tent filter. This reduces visible aliasing.
3. `Tracer::getRadiance()` finds the nearest surface hit. A ray that misses returns black.
4. At a hit, the tracer estimates direct light by sampling points on emissive spheres and checking visibility, then traces a randomly chosen diffuse bounce recursively.
5. `Image::setPixel()` accumulates the sample and updates the running average.
6. After all passes, `Image::save()` gamma-adjusts the average and writes `render.ppm` in the program's current working directory.

## Direct lighting and recursive diffuse bounces

`Tracer::getRadiance()` estimates the light leaving each surface hit. For a diffuse surface, the rendering equation is

```text
L_out(x) = L_emit(x) + (rho / pi) * integral over hemisphere [L_in(x, wi) * max(0, n · wi) dwi]
```

Here `rho` is the surface's RGB diffuse reflectance (`hitObj->color`), `n` is its oriented unit normal, `wi` is an incoming unit direction, and `L_in` is the radiance arriving from that direction. The `rho / pi` term is the Lambertian BRDF; the cosine discounts light arriving at a grazing angle. The code estimates this integral with an explicit light sample and a recursively traced bounce.

### Explicit emitter sampling

When `EMITTER_SAMPLING` is true, the function loops over emissive shapes. It picks a point `lightPos` on each light and forms `lightDirection`, the normalized direction from `hitPos` to that point. `wi = lightDirection.dot(norm)` is the cosine factor `n · wi`; the `wi > 0` check rejects lights below the surface. A ray from the hit point toward the light checks visibility: the sample contributes only when the light itself is the closest intersection, so an intervening object blocks it.

The light subtends a patch of directions at the surface. Approximating that patch as a cone of half-angle `alphaMax`, its solid angle is

```text
Omega = integral(phi=0..2pi, alpha=0..alphaMax) sin(alpha) d(alpha) d(phi)
      = 2 pi (1 - cos(alphaMax))
```

For a sphere of radius `r` with center distance `D`, the tangent right triangle gives `sin(alphaMax) = r / D`, and therefore `cos(alphaMax) = sqrt(1 - r*r/(D*D))`. Substitution gives `Omega = 2*pi*(1 - sqrt(1 - r*r/(D*D)))`.

If emission and the cosine factor are treated as roughly constant across the apparent light, the rendering-equation integral over that light is approximately

```text
L_direct ~= (rho / pi) * Le * max(0, n · wi) * Omega
```

That is the intent of `lightSampling += light->emit * wi * omega * M_1_PI`; `M_1_PI` is `1/pi`, and the surface reflectance is applied immediately afterward in `hitObj->color * lightSampling`.

This is an approximation in the current code, with a few important limitations:

- The sphere formula uses the distance from `hitPos` to the **light center**. The code instead measures from `hitPos` to the sampled surface point `lightPos`, so its `omega` changes with that point and is not the exact subtended solid angle.
- `srad` is fixed at `1.5`. It matches the lights in `complexScene`, but not the radius-8.5 light in `simpleScene`.
- `Sphere::randomPoint()` chooses polar angle `theta` uniformly. That does not sample sphere area uniformly (it over-samples the polar regions), as its TODO comment notes.

So this code illustrates next-event estimation—aiming a visibility ray at a light rather than waiting for a random bounce to find one—but its light-point and solid-angle calculations should not be treated as a fully unbiased estimator.

### Cosine-weighted indirect bounce

After direct lighting, `getRadiance()` creates an orthonormal frame `(u, v, norm)` and samples one direction in the hemisphere above the surface. With independent uniform random values `U1` and `U2`, the code sets `phi = 2*pi*U1` and `r = sqrt(U2)`. In the unit disk, `r` is the radius; on the hemisphere this gives `sin(theta) = r` and `cos(theta) = sqrt(1 - r*r)`. The direction is

```text
d = u*cos(phi)*sin(theta) + v*sin(phi)*sin(theta) + norm*cos(theta)
```

This is cosine-weighted hemisphere sampling, whose probability density is `p(wi) = cos(theta)/pi`. The bounce ray is traced recursively with `getRadiance(Ray(hitPos, d), depth + 1)`.

This sampling density is chosen to match a diffuse BRDF. A one-sample Monte Carlo estimate of the diffuse integral divides its integrand by the sampling density:

```text
[(rho / pi) * L_in(wi) * cos(theta)] / [cos(theta) / pi] = rho * L_in(wi)
```

The cosine and `pi` cancel, which is why the recursive result is multiplied only by `hitObj->color`; there is no additional cosine or `1/pi` factor at the call site. The recursive call performs the same calculation at the next hit, producing indirect illumination through multiple bounces.

### How the terms are combined

With emitter sampling enabled, the returned value is `rho * lightSampling + rho * reflected`, plus `hitObj->emit` only when `depth == 0`. At deeper levels, emissive hits are suppressed; the intent is to account for direct illumination through explicit emitter samples instead. With emitter sampling disabled, the function includes the hit surface's emission at every depth and `lightSampling` stays zero.

There is a further estimator caveat in the Russian roulette termination: after depth 4, a path can be stopped according to `hitObj->color.max()`. A mathematically unbiased roulette estimator scales surviving paths by the inverse survival probability. This implementation does not do that, so the expected contribution from surviving deep paths is reduced.

## Main types and functions

- `Vector` (line 18): 3D vector math, component-wise color operations, dot/cross products, normalization, and clamping.
- `Ray` (line 73): an origin and direction.
- `Image` (line 78): per-pixel accumulated color, sample counts, and PPM output. Pixels are stored bottom-up relative to the render loop.
- `Shape` and `Sphere` (lines 157–217): scene objects. Sphere intersection solves the quadratic ray-sphere equation; normals are computed from the sphere center.
- `simpleScene` and `complexScene` (lines 224–255): Cornell box inspired layouts. Each sphere has a surface color and an emission color; nonzero emission marks a light.
- `Tracer::getIntersection()` (line 262): tests each scene object and returns the closest positive hit.
- `Tracer::getRadiance()` (line 281): recursive light transport, including emitter sampling and Russian roulette termination.
- `main()` (line 386): render settings, camera setup, per-pixel sampling, and final output.

## Settings to explore

In `main()`:

- `w`, `h`: image dimensions (currently 256 × 256).
- `SAMPLES`: number of passes per pixel (currently 4 in the Xcode copy; the root copy uses 50).
- `EMITTER_SAMPLING`: enables explicit direct-light samples.
- `FOCUS_EFFECT`, `FOCAL_LENGTH`: depth-of-field controls; currently disabled.
- `APERTURE_FACTOR`: changes camera view angle and camera shift.
- `scene`: selects the scene to render.

More samples generally reduce Monte Carlo noise at the cost of render time. The output path is relative, so the working directory used to launch the program determines where `render.ppm` appears.

## Differences between the two source copies

The Xcode project compiles `montelight/montelight/montelight.cc`. The README's command-line build compiles root `montelight.cc`. They are not identical. The Xcode copy includes additional explanatory comments and a guard that returns black when a ray misses; its sample count is 4. The root copy has 50 samples and lacks the miss guard. Keep this in mind when a change or render appears in only one build path.

## Things to scrutinize when studying or changing the code

- The miss guard is needed before `getRadiance()` dereferences `hitObj`.
- The `mirror` and `glass` comments in the scene do not correspond to mirror reflection or refraction in the implementation; all non-emissive spheres currently use the same diffuse bounce logic.
- Direct-light estimation hard-codes `srad = 1.5`, matching the lights in `complexScene`, but not the radius-8.5 light in `simpleScene`.
- `Image::getSurroundingAverage()` declares `total` without initializing it. That helper is in the source but is not used by the active render path.
- Snapshot saving is attempted every ten samples into a `temp/` subdirectory. The program does not create that directory; final output is saved as `render.ppm` regardless.

## Suggested reading order

Read `main()` first to see the overall work, then follow the calls into `getRadiance()`, `getIntersection()`, and `Sphere::intersects()`. Return to `Image` to understand accumulation and output. Finally, experiment with one setting at a time and compare the output and runtime.
