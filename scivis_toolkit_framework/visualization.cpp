#include "visualization.h"

#include "constants.h"
#include "mainwindow.h"

#include <QDebug>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>

Visualization::Visualization(QWidget *parent) : QOpenGLWidget(parent)
{
    qDebug() << "Visualization constructor";

    using namespace std::chrono_literals;

    // Start the simulation loop.
    m_timer.start(17ms); // Each frame takes 17ms, making the simulation run at approximately 60 FPS
    connect(&m_timer, SIGNAL(timeout()), this, SLOT(doOneSimulationStep()));

    m_elapsedTimer.start();
}

Visualization::~Visualization()
{
    makeCurrent();

    qDebug() << "Visualization destructor";

    opengl_deleteObjects();
}

void Visualization::doOneSimulationStep()
{
    if (m_isRunning)
        m_simulation.doOneSimulationStep();

    update();
}

void Visualization::initializeGL()
{
    qDebug() << ":: Initializing OpenGL";
    initializeOpenGLFunctions();

    connect(&m_debugLogger, SIGNAL(messageLogged(QOpenGLDebugMessage)),
            this, SLOT(onMessageLogged(QOpenGLDebugMessage)), Qt::DirectConnection);

    if (m_debugLogger.initialize())
    {
        qDebug() << ":: Logging initialized";
        m_debugLogger.startLogging(QOpenGLDebugLogger::SynchronousLogging);
        m_debugLogger.enableMessages();
    }

    {
        QString const glVersion{reinterpret_cast<const char*>(glGetString(GL_VERSION))};
        qDebug() << ":: Using OpenGL" << qPrintable(glVersion);
    }

    glClearColor(0.2F, 0.1F, 0.2F, 1.0F);

    // Retrieve default textures.
    auto const mainWindowPtr = qobject_cast<MainWindow*>(parent()->parent());
    std::vector<Color> const defaultScalarDataColorMap = mainWindowPtr->m_defaultScalarDataColorMap;
    std::vector<Color> const defaultVectorDataColorMap = mainWindowPtr->m_defaultVectorDataColorMap;

    opengl_generateObjects();
    opengl_createShaderPrograms();

    opengl_setupAllBuffers();

    opengl_loadScalarDataTexture(defaultScalarDataColorMap);
    opengl_loadVectorDataTexture(defaultVectorDataColorMap);

    opengl_rotateView();
}

void Visualization::paintGL()
{
    // The height plot, LIC and volume rendering must be drawn by themselves.
    // The scalar data, isolines and vector data drawing can be combined.
    if (m_drawHeightplot)
    {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        opengl_drawHeightplot();
        return;
    }

    // The height plot requires clearing the color buffer *and* depth buffer.
    // The other visualizations only require clearing the color buffer.
    glClear(GL_COLOR_BUFFER_BIT);

    if (m_drawLIC)
    {
        opengl_drawLic();
        return;
    }

    if (m_drawVolumeRendering)
    {
        opengl_drawVolumeRendering();
        return;
    }

    if (m_drawScalarData)
        drawScalarData();

    if (m_drawIsolines)
    {
        m_shaderProgramIsolines.bind();
        glUniform3fv(m_uniformLocationIsolines_color, 1, &m_isolineColor[0]);
        opengl_drawIsolines();
    }

    if (m_drawVectorData)
    {
        m_shaderProgramVectorData.bind();
        glUniform1i(m_uniformLocationTextureColorMapInstanced, 0);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_1D, m_vectorDataTextureLocation);
        drawGlyphs();
    }
}

void Visualization::resizeGL(int const width, int const height)
{
    m_cellWidth  = 2.0F / static_cast<float>(m_DIM + 1U);
    m_cellHeight = 2.0F / static_cast<float>(m_DIM + 1U);

    opengl_updateScalarPoints();

    float const windowRatio = static_cast<float>(width) / static_cast<float>(height);
    m_projectionTransformationMatrix.setToIdentity();
    m_projectionTransformationMatrix.perspective(60.0F, windowRatio, 0.2F, 100.0F);
    m_projectionTransformationMatrix.lookAt({0.0F, 0.0F, 0.0F}, {0.0F, 0.0F, -1.0F}, {0.0F, 1.0F, 0.0F});

    // The OpenGL widget has a total size of width()/height() and a border of m_cellWidth/m_cellHeight pixels.
    // The LIC texture should not include the border.
    // Use int instead of size_t to prevent some type conversions.
    m_licTextureWidth = width - 2 * static_cast<int>(std::round(m_cellWidth));
    m_licTextureHeight = height - 2 * static_cast<int>(std::round(m_cellHeight));

    opengl_updateLicPoints();
    opengl_generateAndLoadLicNoiseTexture();
    setLicStepSize(0.5F);
}

void Visualization::drawGlyphs()
{
    std::vector<float> vectorMagnitude;
    std::vector<float> vectorDirectionX;
    std::vector<float> vectorDirectionY;
    switch (m_currentVectorDataType)
    {
        case VectorDataType::Velocity:
            vectorMagnitude = m_simulation.velocityMagnitudeInterpolated(m_numberOfGlyphsX, m_numberOfGlyphsY);
            vectorDirectionX = m_simulation.velocityXInterpolated(m_numberOfGlyphsX, m_numberOfGlyphsY);
            vectorDirectionY = m_simulation.velocityYInterpolated(m_numberOfGlyphsX, m_numberOfGlyphsY);
        break;

        case VectorDataType::ForceField:
            vectorMagnitude = m_simulation.forceFieldMagnitudeInterpolated(m_numberOfGlyphsX, m_numberOfGlyphsY);
            vectorDirectionX = m_simulation.forceFieldXInterpolated(m_numberOfGlyphsX, m_numberOfGlyphsY);
            vectorDirectionY = m_simulation.forceFieldYInterpolated(m_numberOfGlyphsX, m_numberOfGlyphsY);
        break;
    }

    // Scale the magnitudes to where these become visible.
    std::transform(vectorMagnitude.begin(), vectorMagnitude.end(), vectorMagnitude.begin(),
                   [this] (auto const e) { return m_vectorDataMagnifier * e; } );

    if (m_sendMinMaxToUI && !vectorMagnitude.empty())
    {
        auto const currentMinMaxIt = std::minmax_element(vectorMagnitude.cbegin(), vectorMagnitude.cend());
        QVector2D const currentMinMax{*currentMinMaxIt.first, *currentMinMaxIt.second};

        // Send values to GUI.
        auto const mainWindowPtr = qobject_cast<MainWindow*>(parent()->parent());
        Q_ASSERT(mainWindowPtr != nullptr);
        mainWindowPtr->setVectorDataMin(currentMinMax.x());
        mainWindowPtr->setVectorDataMax(currentMinMax.y());
    }

    size_t const numberOfInstances = m_numberOfGlyphsX * m_numberOfGlyphsY;

    // Create model transformation matrices
    std::vector<float> modelTransformationMatrices;
    /* Fill the container modelTransformationMatrices here...
     * Use the following variables:
     * modelTransformationMatrix: This vector should contain the result.
     * m_DIM: The grid dimensions of the simulation. The simulation uses m_DIM * m_DIM data points.
     * m_cellWidth, m_cellHeight: A cell, made up of 4 simulation data points, has the size m_cellWidth * m_cellHeight for the visualization.
     *                            The border around the visualization also has a width/height of m_cellWidth/m_cellHeight.
     *                            Note: The bottom-left corner of the OpenGL widget has the coordinate (-1, -1),
     *                            so all glyphs should be offset by (m_cellWidth - 1.0F, m_cellHeight - 1.0F).
     * m_numberOfGlyphsX (horizontal)
     * m_numberOfGlyphsY (vertical)
     * vectorDirectionX, vectorDirectionY: To which direction the glyph should point. Row-major, size given by the m_numberOfGlyphs.
     * vectorMagnitude: Use this value to scale the glyphs. I.e. higher values are visualized using larger glyphs. Row-major, size given by the m_numberOfGlyphs.
     */
    modelTransformationMatrices = std::vector<float>(numberOfInstances * 16U, 0.0F); // Remove this placeholder initialization

    // Buffering section starts here.
    glBindVertexArray(m_vaoGlyphs);

    glBindBuffer(GL_ARRAY_BUFFER, m_vboValuesGlyphs);
    glBufferSubData(GL_ARRAY_BUFFER,
                    0,
                    static_cast<GLsizeiptr>(vectorMagnitude.size() * sizeof(float)),
                    vectorMagnitude.data());

    // Buffer model transformation matrices.
    glBindBuffer(GL_ARRAY_BUFFER, m_vboModelTransformationMatricesGlyphs);
    void * const dataPtr = glMapBuffer(GL_ARRAY_BUFFER, GL_WRITE_ONLY);
    memcpy(dataPtr, modelTransformationMatrices.data(), modelTransformationMatrices.size() * sizeof(float));
    glUnmapBuffer(GL_ARRAY_BUFFER);

    if (m_currentGlyphType == Glyph::GlyphType::Hedgehog)
        glDrawElementsInstanced(GL_LINES,
                                static_cast<GLsizei>(m_glyphIndicesSize),
                                GL_UNSIGNED_SHORT,
                                reinterpret_cast<GLvoid*>(0),
                                static_cast<GLsizei>(numberOfInstances));
    else
        glDrawElementsInstanced(GL_TRIANGLE_STRIP,
                                static_cast<GLsizei>(m_glyphIndicesSize),
                                GL_UNSIGNED_SHORT,
                                reinterpret_cast<GLvoid*>(0),
                                static_cast<GLsizei>(numberOfInstances));
}


void Visualization::applyQuantization(std::vector<float> &scalarValues) const
{
    // Convert the floating point values to (8 bit) unsigned integers,
    // so that the data can be treated as an image.
    // The image's pixel values are in the range [0, 255].
    float const maxValue = *std::max_element(scalarValues.cbegin(), scalarValues.cend());
    std::vector<unsigned int> image;
    image.reserve(scalarValues.size());
    for (auto const x : scalarValues)
        image.push_back(static_cast<unsigned int>(std::lroundf(x / maxValue * 255.0F)));


    // Apply quantization to std::vector<unsigned int> image here.
    // The variable m_quantizationBits ('n' in the lecture slides) is set in the GUI and can be used here.
    // L needs to be set to the appropriate value and will be used to set the clamping range in the GUI.

    unsigned int const L = 1U; // placeholder value
    qDebug() << "Quantization not implemented";

    // Convert the image's data back to floating point values, so that it can be processed as usual.
    scalarValues = std::vector<float>{image.cbegin(), image.cend()};

    // Force the clamping range in the GUI to be [0, L].
    auto const mainWindowPtr = qobject_cast<MainWindow*>(parent()->parent());
    Q_ASSERT(mainWindowPtr != nullptr);
    mainWindowPtr->on_scalarDataMappingClampingMaxSlider_valueChanged(0);
    mainWindowPtr->on_scalarDataMappingClampingMaxSlider_valueChanged(100 * static_cast<int>(L));
}

void Visualization::applyGaussianBlur(std::vector<float> &scalarValues) const
{
    // Implement Gaussian blur here, applied on the values of the scalarValues container.
    // First, define a 3x3 matrix for the kernel.
    // (Use a C-style 2D array, a std::array of std::array's, or a std::vector of std::vectors)

    qDebug() << "Gaussian blur not implemented";
}

void Visualization::applyGradients(std::vector<float> &scalarValues) const
{
    // Implement Gradient extraction here, applied on the values of the scalarValues container.
    // First, define a 3x3 Sobel kernels (for x and y directions).
    // (Use a C-style 2D array, a std::array of std::array's, or a std::vector of std::vectors)
    // Convolve the values of the scalarValues container with the Sobel kernels
    // Calculate the Gradient magnitude
    // Calculate the Gradient direction
    // Visualize the Gradient magnitude

    qDebug() << "applyGradients not implemented";
}

/* This function receives a *reference* to a std::vector<float>,
 * which acts as a pointer. Modifying scalarValues here will result
 * in the scalarValues passed to this function to be modified.
 * You may also define a new "std::vector<float> v" here, fill it with
 * new values and assign it to scalarValues to replace it,
 * e.g. "scalarValues = v;".
 *
 * m_sliceIdx contains the value set in the GUI.
 * m_DIM contains the current dimensions of the square (m_DIM * m_DIM).
 * m_slicingWindowSize contains the size of the window (here, also m_DIM).
 * m_slicingDirection contains the slicing direction set in the GUI and
 *    is already handled in a switch statement.
 */
void Visualization::applySlicing(std::vector<float> &scalarValues)
{
    qDebug() << "Slicing not implemented";
    // Add code here and below to complete the implementation

    switch (m_slicingDirection)
    {
    case SlicingDirection::x:
        // xIdx is constant
        qDebug() << "Slicing in x not implemented";
        break;

    case SlicingDirection::y:
        // yIdx is constant
        qDebug() << "Slicing in y not implemented";
        break;

    case SlicingDirection::t:
        // t is constant
        qDebug() << "Slicing in t not implemented";
        break;
    }
}

void Visualization::applyPreprocessing(std::vector<float> &scalarValues)
{
    if (m_useQuantization)
        applyQuantization(scalarValues);

    if (m_useGaussianBlur)
        applyGaussianBlur(scalarValues);

    if (m_useGradients)
        applyGradients(scalarValues);

    if (m_useSlicing)
        applySlicing(scalarValues);
}

void Visualization::drawScalarData()
{
    std::vector<float> scalarValues;

    switch (m_currentScalarDataType)
    {
        case ScalarDataType::Density:
            scalarValues = m_simulation.density();
        break;

        case ScalarDataType::ForceFieldMagnitude:
            scalarValues = m_simulation.forceFieldMagnitude();
        break;

        case ScalarDataType::VelocityMagnitude:
            scalarValues = m_simulation.velocityMagnitude();
        break;

        case ScalarDataType::VelocityDivergence:
            scalarValues = velocityDivergence();
        break;

        case ScalarDataType::ForceFieldDivergence:
            scalarValues = forceFieldDivergence();
        break;
    }

    applyPreprocessing(scalarValues);
    opengl_drawScalarData(scalarValues);
}

std::vector<float> Visualization::velocityDivergence() const
{
    std::vector<float> velocityDivergence;
    velocityDivergence.resize(m_DIM * m_DIM);

    auto const backwardFiniteDifference = [&](size_t const idx, size_t const previousX_idx, size_t const previousY_idx)
    {
        return (m_simulation.vx(idx) - m_simulation.vx(previousX_idx)) / m_cellWidth +
               (m_simulation.vy(idx) - m_simulation.vy(previousY_idx)) / m_cellHeight;
    };

    velocityDivergence.at(0) = backwardFiniteDifference(0, m_DIM - 1, (m_DIM - 1) * m_DIM);
    for (size_t idx = 1; idx < m_DIM; ++idx)
        velocityDivergence.at(idx) = backwardFiniteDifference(idx, idx - 1, idx + (m_DIM - 1) * m_DIM);

    for (size_t idx = m_DIM; idx < (m_DIM - 1) * m_DIM; ++idx)
    {
        if (idx % m_DIM == 0)
            velocityDivergence.at(idx) = backwardFiniteDifference(idx, idx + m_DIM - 1, idx - m_DIM);
        else
            velocityDivergence.at(idx) = backwardFiniteDifference(idx, idx - 1, idx - m_DIM);
    }

    velocityDivergence.at((m_DIM - 1) * m_DIM) = backwardFiniteDifference((m_DIM - 1) * m_DIM, m_DIM * m_DIM - 1, 0);
    for (size_t idx = (m_DIM - 1) * m_DIM + 1; idx < m_DIM * m_DIM; ++idx)
        velocityDivergence.at(idx) = backwardFiniteDifference(idx, idx - 1, idx - (m_DIM - 1) * m_DIM);

    return velocityDivergence;
}

std::vector<float> Visualization::forceFieldDivergence() const
{
    std::vector<float> forceFieldDivergence;
    forceFieldDivergence.resize(m_DIM * m_DIM);

    auto const backwardFiniteDifference = [&](size_t const idx, size_t const previousX_idx, size_t const previousY_idx)
    {
        return (m_simulation.fx(idx) - m_simulation.fx(previousX_idx)) / m_cellWidth +
               (m_simulation.fy(idx) - m_simulation.fy(previousY_idx)) / m_cellHeight;
    };

    forceFieldDivergence.at(0) = backwardFiniteDifference(0, m_DIM - 1, (m_DIM - 1) * m_DIM);
    for (size_t idx = 1; idx < m_DIM; ++idx)
        forceFieldDivergence.at(idx) = backwardFiniteDifference(idx, idx - 1, idx + (m_DIM - 1) * m_DIM);

    for (size_t idx = m_DIM; idx < (m_DIM - 1) * m_DIM; ++idx)
    {
        if (idx % m_DIM == 0)
            forceFieldDivergence.at(idx) = backwardFiniteDifference(idx, idx + m_DIM - 1, idx - m_DIM);
        else
            forceFieldDivergence.at(idx) = backwardFiniteDifference(idx, idx - 1, idx - m_DIM);
    }

    forceFieldDivergence.at((m_DIM - 1) * m_DIM) = backwardFiniteDifference((m_DIM - 1) * m_DIM, m_DIM * m_DIM - 1, 0);
    for (size_t idx = (m_DIM - 1) * m_DIM + 1; idx < m_DIM * m_DIM; ++idx)
        forceFieldDivergence.at(idx) = backwardFiniteDifference(idx, idx - 1, idx - (m_DIM - 1) * m_DIM);

    return forceFieldDivergence;
}

std::vector<QVector3D> Visualization::computeNormals(std::vector<float> heights) const
{
    return std::vector<QVector3D>(heights.size(), QVector3D(0.0F, 0.0F, 1.0F));
}

namespace
{
    QVector4D transferFunction(float value)
    {
        // Define colors for the colormap
        QVector3D const colorNode0{0.0F, 0.0F, 1.0F}; // blue
        // QVector3D const colorNode1{1.0F, 1.0F, 1.0F}; // white
        QVector3D const colorNode1{0.0F, 1.0F, 0.0F}; // green
        QVector3D const colorNode2{1.0F, 0.0F, 0.0F}; // red

        value /= 255.0F; // to range [0...1]

        float alpha = value * 0.5F; // value;
        if (value < 0.2F)
            alpha = 0.5F; // 0.0F;

        QVector3D color0 = colorNode0;
        QVector3D color1 = colorNode1;

        float t = 0.0F;
        if (value < 0.5F)
        {
            t = 2.0F * value;
        }
        else
        {
            t = 2.0F * (value - 0.5F);
            color0 = colorNode1;
            color1 = colorNode2;
        }

        QVector4D color;

        color[3] = alpha;

        for (int idx = 0; idx < 3; ++idx) // rgb
            color[idx] = color0[idx] * (1.0F - t) + color1[idx] * t;

        return color;
    }

    float opacityCorrection(float const alpha, float const sampleRatio)
    {
         return 1.0F - std::pow(1.0F - alpha, sampleRatio);
    }
}

std::vector<QVector4D> Visualization::computePreIntegrationLookupTable(size_t const DIM) const
{
    unsigned int const L = 100U; // total number of steps from 0 to delta-t

    // TODO: modify the transferFunction and add necessary functions

    // placeholder values
    std::vector<QVector4D> lookupTable;
    for (size_t idx = 0U; idx < DIM * DIM; ++idx)
        lookupTable.push_back({0.5F, 0.5F, 0.5F, 1.0F});

    return lookupTable;
}

void Visualization::onMessageLogged(QOpenGLDebugMessage const &Message) const
{
    qDebug() << "Log from Visualization:" << Message;
}

// Setters
void Visualization::setDIM(size_t const DIM)
{
    // Stop the simulation, do all resizing, then continue.
    m_timer.stop();

    m_DIM = DIM;
    m_numberOfGlyphsX = m_DIM;
    m_numberOfGlyphsY = m_DIM;
    opengl_setupAllBuffers();
    resizeGL(width(), height());
    m_simulation.setDIM(m_DIM);

    m_timer.start();
}

void Visualization::setNumberOfGlyphsX(size_t const numberOfGlyphsX)
{
    m_numberOfGlyphsX = numberOfGlyphsX;
    opengl_setupGlyphsPerInstanceData();
}

void Visualization::setNumberOfGlyphsY(size_t const numberOfGlyphsY)
{
    m_numberOfGlyphsY = numberOfGlyphsY;
    opengl_setupGlyphsPerInstanceData();
}

void Visualization::setLicStepSize(float const stepSizeFactor)
{
    // Assuming width == height
    m_licStepSize = stepSizeFactor * (1.0F / static_cast<float>(m_licTextureWidth));
}
