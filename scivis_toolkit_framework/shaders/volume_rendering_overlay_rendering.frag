#version 330 core
// volume_rendering_overlay_rendering fragment shader

in vec2 uv;

uniform vec2 iResolution;
uniform float iTime;
uniform sampler3D textureSampler;

out vec4 color;

// Color map for the time steps
const vec3 colorTimeStep0 = vec3(0.19483F, 0.08339F, 0.26149F);
const vec3 colorTimeStep1 = vec3(0.276877F, 0.467233F, 0.938261F);
const vec3 colorTimeStep2 = vec3(0.111057F, 0.80685F, 0.843763F);
const vec3 colorTimeStep3 = vec3(0.392349F, 0.99044F, 0.41466F);
const vec3 colorTimeStep4 = vec3(0.816737F, 0.916873F, 0.205837F);
const vec3 colorTimeStep5 = vec3(0.99648F, 0.600076F, 0.174426F);
const vec3 colorTimeStep6 = vec3(0.857794F, 0.226128F, 0.0279014F);
const vec3 colorTimeStep7 = vec3(0.49321F, 0.0196299F, 0.00955002F);
const vec3 colorsTimeStep[8] = vec3[8](colorTimeStep0, colorTimeStep1, colorTimeStep2, colorTimeStep3, colorTimeStep4, colorTimeStep5, colorTimeStep6, colorTimeStep7);

// bounding box
const vec3 bbMin = vec3(-0.5F);
const vec3 bbMax = vec3(0.5F);

// additional camera parameters
const float fovy = 58.0F * (3.14159267F / 180.0F); // degrees converted to radians
const float zNear = 0.1F;

// light direction
const vec3 lightDir = vec3(0.0F, -1.0F, -1.0F);

const vec4 lightColor = vec4(1.0F);
const vec4 specularColor = vec4(1.0F);
const float ka = 0.5F;  // ambient contribution
const float kd = 0.5F;  // diffuse contribution
const float ks = 0.7F;  // specular contribution
const float exponent = 50.0F;  // specular exponent (shininess)

// number of maximum raycasting samples per ray
const int sampleNum = 150;

// width of one voxel
const float voxelWidth = 1.0F / 64.0F;

// Colors for the colormap
const vec3 colorNode0 = vec3(0.0F, 0.0F, 1.0F);  // blue
//const vec3 colorNode1 = vec3(1.0F, 1.0F, 1.0F);  // white
const vec3 colorNode1 = vec3(0.0F, 1.0F, 0.0F);  // green
const vec3 colorNode2 = vec3(1.0F, 0.0F, 0.0F);  // red

/**
 *	Samples the volume texture at a given position.
 *
 *	@param texCoord The position one wants to retrieve the sample of (in world coordinates).
 *	@return The sample value at the given position.
 */
float sampleVolume(vec3 texCoord)
{
    return texture(textureSampler, texCoord).r;
}

/**
 *	Evaluates the transfer function for a given sample value
 *
 *	@param value The sample value
 *	@return The color for the given sample value
 */
vec4 transferFunction(float value)
{
    if (value < 0.5F)
        return vec4(0.0F);

    return vec4(value);
}

/**
 *	Intersects a ray with the bounding box and returns the intersection points
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
 *	Correct opacity for the current sampling rate
 *
 *	@param alpha The input opacity.
 *	@param samplingRatio The ratio between current sampling rate and the original one.

 */
float opacityCorrection(float alpha, float samplingRatio)
{
    float a_corrected = 1.0F - pow(1.0F - alpha, samplingRatio);
    return a_corrected;
}

/**
 * Accumulation composition
 *
 * @param value The current sample value.
 * @param opacityCorrectionFactor The ratio between current sampling rate and the original one.
 * @param composedColor The blended color (both input and output).
 */
void accumulation(float value, float opacityCorrectionFactor, inout vec4 composedColor)
{
    vec4 color = transferFunction(value);
    color.a = opacityCorrection(color.a, opacityCorrectionFactor);

    // TODO: Implement Front-to-back blending
    composedColor = vec4(0.5) * value; // placeholder
}

/**
 * Main Function: Computes the color for the given fragment.
 *
 * @param fragColor OUT The color of the pixel / fragment.
 */
void mainImage(out vec4 fragColor)
{
    float aspect = iResolution.x / iResolution.y;

    /******************** compute camera parameters ********************/

    // camera movement
    float camSpeed = 0.5F;
    vec3 camPos = 1.5F * vec3(cos(iTime * camSpeed), 0.5F, sin(iTime * camSpeed));
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
    vec4 background = vec4(0.0F, 0.0F, 0.0F, 1.0F);
    if (!hit)
    {
        fragColor = background;
        return;
    }

    float rayStepSize = (bbMax.x - bbMin.x) / float(sampleNum);
    vec4 finalColor = vec4(0.0F);
    vec3 finalGradient = vec3(0.0F);
    // ratio between current sampling rate vs. the original sampling rate
    float opacityCorrectionFactor = 1.0F / (float(sampleNum) * voxelWidth);

    /******************** main raycasting loop *******************/
    float t = tNear;
    int i = 0;
    while(t < tFar && i < sampleNum)
    {
        vec3 pos = camPos + t * rayDir;
        // Use normalized volume coordinate
        vec3 texCoord = pos + 0.5F;
        float value = sampleVolume(texCoord);

        accumulation(value, opacityCorrectionFactor, finalColor);

        t += rayStepSize;
    }

    fragColor.rgb = mix(background.rgb, finalColor.rgb, finalColor.a);
    fragColor.a = 1.0F;
}

void main()
{
    mainImage(color);
}
