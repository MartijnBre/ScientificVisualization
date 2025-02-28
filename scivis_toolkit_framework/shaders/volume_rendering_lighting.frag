#version 330 core
// volume_rendering_lighting fragment shader

// TODO If you want to change from central to intermediate differences, do it here by commenting/uncommenting the corresponding #define
//#define USE_CENTRAL
#define USE_INTERMEDIATE

in vec2 uv;

uniform vec2 iResolution;
uniform float iTime;

out vec4 color;

// first three coordinates: center position
// w-component: radius for spheres, half-width for cubes
const vec4 cube1 = vec4(0.0F, 0.0F, 0.0F, 1.0F);
const vec4 cube2 = vec4(1.0F, 1.0F, 1.0F, 1.5F);
const vec4 sphere1 = vec4(1.0F, 0.0F, 3.0F, 0.5F);

// density of objects
const float cube1Density = 0.015F;
const float cube2Density = 0.02F;
const float sphere1Density = 0.03F;

// bounding box
// These are the dimensions in which the objects exists...
// const vec3 bbMin = vec3(-1.5F, -1.5F, -1.5F);
// const vec3 bbMax = vec3(2.5F, 2.5F, 3.5F);
// However, the bounding box intersection algorithm requires a cube as a bounding box:
const vec3 bbMin = vec3(-3.5F);
const vec3 bbMax = vec3(3.5F);

// additional camera parameters
const float fovy = 58.0F * (3.14159267F / 180.0F); // degrees converted to radians
const float zNear = 0.1F;

// light direction
const vec3 lightDir = vec3(1.0F, 1.0F, 1.0F);

const vec4 lightColor = vec4(1.0F);
const vec4 specularColor = vec4(1.0F);
const float ka = 0.5F;  // ambient contribution
const float kd = 0.5F;  // diffuse contribution
const float ks = 0.7F;  // specular contribution
const float exponent = 50.0F;  // specular exponent (shininess)

// number of maximum raycasting samples per ray
const int sampleNum = 256;

// width of one voxel
const float voxelWidth = 1.0F / 64.0F;

// epsilon for comparisons
const float EPS = 1.0E-7F;

/**
 *	Returns whether a given point is inside a given sphere.
 *
 * 	@param point The point that is tested against the sphere.
 * 	@param sphere The sphere parameters. xyz = position, w = radius
 *	@return True, when the point is inside the sphere, false otherwise
 */
bool isInSphere(vec3 point, vec4 sphere)
{
    vec3 spherePos = sphere.xyz;
    return length(point - spherePos) <= sphere.w;
}

/**
 *	Returns whether a given point is inside a given cube.
 *
 * 	@param point The point that is tested against the cube.
 * 	@param cube The cube parameters. xyz = position, w = half of the cube width
 *	@return True, when the point is inside the cube, false otherwise
 */
bool isInCube(vec3 point, vec4 cube)
{
    vec3 dist = abs(point.xyz - cube.xyz);
    return all(lessThan(dist, vec3(cube.w)));
}

/**
 *	Samples the volume texture at a given position.
 *
 *	@param volumeCoord The position one wants to retrieve the sample of (in world coordinates).
 *	@return The sample value at the given position.
 */
float sampleVolume(vec3 volumeCoord)
{
    bool in1 = isInCube(volumeCoord, cube1);
    bool in2 = isInCube(volumeCoord, cube2);
    bool in3 = isInSphere(volumeCoord, sphere1);

    float result = 0.0F;

    if (in1)
        result += cube1Density;

    if (in2)
        result += cube2Density;

    if (in3)
        result += sphere1Density;

    return result;
}

/**
 *	Evaluates the transfer function for a given sample value
 *
 *	@param value The sample value
 *	@return The color for the given sample value
 */
vec4 transferFunction(float value)
{
    if (value > EPS)
    {
        if (value > cube1Density + EPS)
        {
            if (value > cube2Density + EPS)
            {
                if (value > cube1Density + cube2Density + EPS)
                {
                    return vec4(0.0F, 0.0F, 0.0F, 1.0F);
                }
                return vec4(1.0F, 0.0F, 0.0F, 1.0F);
            }
            return vec4(0.0F, 1.0F, 0.0F, 1.0F);
        }
        return vec4(0.0F, 0.0F, 1.0F, 1.0F);
    }
    return vec4(0.0F);
}

/**
 *	Intersects a ray with the bounding box and returs the intersection points
 *
 * 	@param rayOrig The origin of the ray
 * 	@param rayDir The direction of the ray
 *  @param tNear OUT The distance from the ray origin to the first intersection point
 *  @param tFar OUT The distance from the ray origin to the second intersection point
 *  @return True if the ray intersects the bounding box, false otherwise.
 */
bool intersectBoundingBox(vec3 rayOrig, vec3 rayDir, out float tNear, out float tFar)
{
    vec3 invR = vec3(1.0F) / rayDir;
    vec3 tbot = invR * (bbMin - rayOrig);
    vec3 ttop = invR * (bbMax - rayOrig);

    vec3 tmin = min(ttop, tbot);
    vec3 tmax = max(ttop, tbot);

    float largestTMin = max(max(tmin.x, tmin.y), max(tmin.x, tmin.z));
    float smallestTMax = min(min(tmax.x, tmax.y), min(tmax.x, tmax.z));

    tNear = largestTMin;
    tFar = smallestTMax;

    return smallestTMax > largestTMin;
}

/**
 *	Returns the gradient at a given position using central differences
 *	@param pos The postion from which the gradient should be determined
 *	@return The gradient at pos.
 */
vec3 gradientCentral(vec3 pos)
{
    vec3 result;
    //TODO: Insert code here
    return result;
}

/**
 *	Returns the gradient at a given position using intermediate differences
 *
 *	@param pos The postion from which the gradient should be determined
 *	@return The gradient at pos.
 */
vec3 gradientIntermediate(vec3 pos)
{
    vec3 result;
    //TODO: Insert code here
    return result;
}

/**
 *	Computes the color of the lit surface of an object, using a global
 *	directional light source.
 *
 *	@param diffuseColor The diffuse color of the object.
 *	@param normal The surface normal at the position that should be lit.
 *	@param eyeDir The direction from the surface to the camera position.
 *	@return The color of the lit surface
 */
vec4 lighting(vec4 diffuseColor, vec3 normal, vec3 eyeDir)
{
    // TODO Insert code here
    vec4 color = diffuseColor;
    return color;
}

/**
 *	Main Function:
 *  Computes the color for the given fragment.
 *
 *	@param fragColor OUT The color of the pixel / fragment.
 */
void mainImage(out vec4 fragColor)
{
    float aspect = iResolution.x / iResolution.y;

    /******************** compute camera parameters ********************/

    // camera movement
    float camSpeed = 0.5F;
    vec3 camPos = 7.0F * vec3(cos(iTime * camSpeed), 0.5F, sin(iTime * camSpeed));
    vec3 camDir = -normalize(camPos);
    vec3 camUp = vec3(0.0F, 1.0F, 0.0F);
    vec3 camRight = normalize(cross(camDir, camUp));
    camUp = normalize(cross(camRight, camDir));

    /************ compute ray direction (OpenGL style) *****************/
    float fovx = 2.0F * atan(tan(fovy / 2.0F) * aspect);

    vec3 uL = (tan(fovx * 0.5F) * zNear) * (-camRight) + (tan(fovy * 0.5F) * zNear) * camUp + camDir * zNear + camPos;
    vec3 lL = (tan(fovx * 0.5F) * zNear) * (-camRight) + (tan(fovy * 0.5F) * zNear) * (-camUp) + camDir * zNear + camPos;
    vec3 uR = (tan(fovx * 0.5F) * zNear) * camRight + (tan(fovy * 0.5F) * zNear) * camUp + camDir * zNear + camPos;
    vec3 lR = (tan(fovx * 0.5F) * zNear) * camRight + (tan(fovy * 0.5F) * zNear) * (-camUp) + camDir * zNear + camPos;

    vec3 targetL = mix(lL, uL, uv.y);
    vec3 targetR = mix(lR, uR, uv.y);
    vec3 target = mix(targetL, targetR, uv.x);

    vec3 rayDir = normalize(target - camPos);

    /******************* test against bounding box ********************/
    float tNear;
    float tFar;
    bool hit = intersectBoundingBox(camPos, rayDir, tNear, tFar);
    // vec4 background = vec4(1.0);
    vec4 background = vec4(0.0F, 0.0F, 0.0F, 1.0F);
    if (tNear < 0.0F)
        tNear = 0.0F;

    if (!hit)
    {
        fragColor = background;
        return;
    }

    vec3 pos = camPos + rayDir * tNear;
    float rayStepSize = (bbMax.x - bbMin.x) / float(sampleNum);
    vec4 finalColor = vec4(0.0F);
    vec3 finalGradient = vec3(0.0F);

    /******************** main raycasting loop *******************/
    for (int i = 0; i < sampleNum; ++i)
    {
        if (finalColor.a > 0.99F)
            break; // early ray termination!

        pos += rayStepSize * rayDir;
        float sampleValue = sampleVolume(pos);
        vec4 color = transferFunction(sampleValue);


        #ifdef USE_INTERMEDIATE
        vec3 grad = gradientIntermediate(pos);
        #else
        #ifdef USE_CENTRAL
        vec3 grad = gradientCentral(pos);
        #else
        vec3 grad = vec3(0.0F);
        #endif // USE_CENTRAL
        #endif // USE_INTERMEDIATE


     /****************** lighting ********************************/
        {
           finalGradient = grad;
        }
        color = lighting(color, -normalize(finalGradient), -rayDir);
        // blending with pre-multiplied color!
        color.rgb *= color.a;
        finalColor += color * (1.0F - finalColor.w);
    }
    fragColor = finalColor * finalColor.a + (1.0F - finalColor.a) * background;
}

void main()
{
    mainImage(color);
}
