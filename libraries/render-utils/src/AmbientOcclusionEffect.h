//
//  AmbientOcclusionEffect.h
//  libraries/render-utils/src/
//
//  Created by Niraj Venkat on 7/15/15.
//  Copyright 2015 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_AmbientOcclusionEffect_h
#define hifi_AmbientOcclusionEffect_h

#include <string>
#include <DependencyManager.h>

#include "render/DrawTask.h"

#include "DeferredFrameTransform.h"
#include "DeferredFramebuffer.h"
#include "SurfaceGeometryPass.h"

#include "ssao_shared.h"

class AmbientOcclusionFramebuffer {
public:
    AmbientOcclusionFramebuffer();

    gpu::FramebufferPointer getOcclusionFramebuffer();
    gpu::TexturePointer getOcclusionTexture();
    
    gpu::FramebufferPointer getOcclusionBlurredFramebuffer();
    gpu::TexturePointer getOcclusionBlurredTexture();

    gpu::FramebufferPointer getNormalFramebuffer();
    gpu::TexturePointer getNormalTexture();

#if SSAO_USE_QUAD_SPLIT
    gpu::FramebufferPointer getOcclusionSplitFramebuffer(int index);
    gpu::TexturePointer getOcclusionSplitTexture();
#endif

    // Update the source framebuffer size which will drive the allocation of all the other resources.
    bool update(const gpu::TexturePointer& linearDepthBuffer, int resolutionLevel, int depthResolutionLevel, bool isStereo);
    gpu::TexturePointer getLinearDepthTexture();
    const glm::ivec2& getSourceFrameSize() const { return _frameSize; }
    bool isStereo() const { return _isStereo; }

protected:

    void clear();
    void allocate();
    
    gpu::TexturePointer _linearDepthTexture;
    
    gpu::FramebufferPointer _occlusionFramebuffer;
    gpu::TexturePointer _occlusionTexture;

    gpu::FramebufferPointer _occlusionBlurredFramebuffer;
    gpu::TexturePointer _occlusionBlurredTexture;

    gpu::FramebufferPointer _normalFramebuffer;
    gpu::TexturePointer _normalTexture;

#if SSAO_USE_QUAD_SPLIT
    gpu::FramebufferPointer _occlusionSplitFramebuffers[SSAO_SPLIT_COUNT*SSAO_SPLIT_COUNT];
    gpu::TexturePointer _occlusionSplitTexture;
#endif

    glm::ivec2 _frameSize;
    int _resolutionLevel{ 0 };
    int _depthResolutionLevel{ 0 };
    bool _isStereo{ false };
};

using AmbientOcclusionFramebufferPointer = std::shared_ptr<AmbientOcclusionFramebuffer>;

class AmbientOcclusionEffectConfig : public render::GPUJobConfig::Persistent {
    Q_OBJECT
    Q_PROPERTY(bool enabled MEMBER enabled NOTIFY dirty)
    Q_PROPERTY(bool horizonBased MEMBER horizonBased NOTIFY dirty)
    Q_PROPERTY(bool ditheringEnabled MEMBER ditheringEnabled NOTIFY dirty)
    Q_PROPERTY(bool borderingEnabled MEMBER borderingEnabled NOTIFY dirty)
    Q_PROPERTY(bool fetchMipsEnabled MEMBER fetchMipsEnabled NOTIFY dirty)
    Q_PROPERTY(float radius MEMBER radius WRITE setRadius)
    Q_PROPERTY(float obscuranceLevel MEMBER obscuranceLevel WRITE setObscuranceLevel)
    Q_PROPERTY(float falloffAngle MEMBER falloffAngle WRITE setFalloffAngle)
    Q_PROPERTY(float edgeSharpness MEMBER edgeSharpness WRITE setEdgeSharpness)
    Q_PROPERTY(float blurDeviation MEMBER blurDeviation WRITE setBlurDeviation)
    Q_PROPERTY(float numSpiralTurns MEMBER numSpiralTurns WRITE setNumSpiralTurns)
    Q_PROPERTY(int numSamples MEMBER numSamples WRITE setNumSamples)
    Q_PROPERTY(int resolutionLevel MEMBER resolutionLevel WRITE setResolutionLevel)
    Q_PROPERTY(int blurRadius MEMBER blurRadius WRITE setBlurRadius)

public:
    AmbientOcclusionEffectConfig();

    const int MAX_RESOLUTION_LEVEL = 4;
    const int MAX_BLUR_RADIUS = 15;

    void setRadius(float newRadius) { radius = std::max(0.01f, newRadius); emit dirty(); }
    void setObscuranceLevel(float level) { obscuranceLevel = std::max(0.01f, level); emit dirty(); }
    void setFalloffAngle(float bias) { falloffAngle = std::max(0.0f, std::min(bias, 1.0f)); emit dirty(); }
    void setEdgeSharpness(float sharpness) { edgeSharpness = std::max(0.0f, (float)sharpness); emit dirty(); }
    void setBlurDeviation(float deviation) { blurDeviation = std::max(0.0f, deviation); emit dirty(); }
    void setNumSpiralTurns(float turns) { numSpiralTurns = std::max(0.0f, (float)turns); emit dirty(); }
    void setNumSamples(int samples) { numSamples = std::max(1.0f, (float)samples); emit dirty(); }
    void setResolutionLevel(int level) { resolutionLevel = std::max(0, std::min(level, MAX_RESOLUTION_LEVEL)); emit dirty(); }
    void setBlurRadius(int radius) { blurRadius = std::max(0, std::min(MAX_BLUR_RADIUS, radius)); emit dirty(); }

    float radius;
    float perspectiveScale;
    float obscuranceLevel; // intensify or dim down the obscurance effect
    float falloffAngle;
    float edgeSharpness;
    float blurDeviation;
    float numSpiralTurns; // defining an angle span to distribute the samples ray directions
    int numSamples;
    int resolutionLevel;
    int blurRadius; // 0 means no blurring
    bool horizonBased; // Use horizon based AO
    bool ditheringEnabled; // randomize the distribution of taps per pixel, should always be true
    bool borderingEnabled; // avoid evaluating information from non existing pixels out of the frame, should always be true
    bool fetchMipsEnabled; // fetch taps in sub mips to otpimize cache, should always be true

signals:
    void dirty();
};

class AmbientOcclusionEffect {
public:
    using Inputs = render::VaryingSet3<DeferredFrameTransformPointer, DeferredFramebufferPointer, LinearDepthFramebufferPointer>;
    using Outputs = render::VaryingSet2<AmbientOcclusionFramebufferPointer, gpu::BufferView>;
    using Config = AmbientOcclusionEffectConfig;
    using JobModel = render::Job::ModelIO<AmbientOcclusionEffect, Inputs, Outputs, Config>;

    AmbientOcclusionEffect();

    void configure(const Config& config);
    void run(const render::RenderContextPointer& renderContext, const Inputs& inputs, Outputs& outputs);

    // Class describing the uniform buffer with all the parameters common to the AO shaders
    class AOParameters : public AmbientOcclusionParams {
    public:

        AOParameters();

        int getResolutionLevel() const { return _resolutionInfo.x; }
        float getRadius() const { return _radiusInfo.x; }
        float getPerspectiveScale() const { return _resolutionInfo.z; }
        float getObscuranceLevel() const { return _radiusInfo.w; }
        float getFalloffAngle() const { return (float)_ditheringInfo.z; }
        
        float getNumSpiralTurns() const { return _sampleInfo.z; }
        int getNumSamples() const { return (int)_sampleInfo.x; }
        bool isFetchMipsEnabled() const { return _sampleInfo.w; }

        bool isDitheringEnabled() const { return _ditheringInfo.x != 0.0f; }
        bool isBorderingEnabled() const { return _ditheringInfo.w != 0.0f; }
        bool isHorizonBased() const { return _resolutionInfo.y != 0.0f; }

    };
    using AOParametersBuffer = gpu::StructBuffer<AOParameters>;

private:

    // Class describing the uniform buffer with all the parameters common to the bilateral blur shaders
    class BlurParameters : public AmbientOcclusionBlurParams {
    public:

        BlurParameters();

        float getEdgeSharpness() const { return (float)_blurInfo.x; }
        int getBlurRadius() const { return (int)_blurInfo.y; }

    };
    using BlurParametersBuffer = gpu::StructBuffer<BlurParameters>;

    using FrameParametersBuffer = gpu::StructBuffer< AmbientOcclusionFrameParams>;

    void updateBlurParameters();
    void updateFramebufferSizes();

    int getDepthResolutionLevel() const;
   
    AOParametersBuffer _aoParametersBuffer;
    FrameParametersBuffer _aoFrameParametersBuffer[SSAO_SPLIT_COUNT*SSAO_SPLIT_COUNT];
    BlurParametersBuffer _vblurParametersBuffer;
    BlurParametersBuffer _hblurParametersBuffer;

    static const gpu::PipelinePointer& getOcclusionPipeline();
	static const gpu::PipelinePointer& getBilateralBlurPipeline();
	static const gpu::PipelinePointer& getMipCreationPipeline();
    static const gpu::PipelinePointer& getGatherPipeline();
    static const gpu::PipelinePointer& getBuildNormalsPipeline();

    static gpu::PipelinePointer _occlusionPipeline;
	static gpu::PipelinePointer _bilateralBlurPipeline;
    static gpu::PipelinePointer _mipCreationPipeline;
    static gpu::PipelinePointer _gatherPipeline;
    static gpu::PipelinePointer _buildNormalsPipeline;

    AmbientOcclusionFramebufferPointer _framebuffer;
    std::array<float, 3 * SSAO_SPLIT_COUNT*SSAO_SPLIT_COUNT> _randomSamples;
    int _frameId{ 0 };
    
    gpu::RangeTimerPointer _gpuTimer;

    friend class DebugAmbientOcclusion;
};


class DebugAmbientOcclusionConfig : public render::Job::Config {
    Q_OBJECT

    Q_PROPERTY(bool showCursorPixel MEMBER showCursorPixel NOTIFY dirty)
    Q_PROPERTY(glm::vec2 debugCursorTexcoord MEMBER debugCursorTexcoord NOTIFY dirty)
public:
    DebugAmbientOcclusionConfig() : render::Job::Config(false) {}

    bool showCursorPixel{ false };
    glm::vec2 debugCursorTexcoord{ 0.5f, 0.5f };

signals:
    void dirty();
};


class DebugAmbientOcclusion {
public:
    using Inputs = render::VaryingSet4<DeferredFrameTransformPointer, DeferredFramebufferPointer, LinearDepthFramebufferPointer, AmbientOcclusionEffect::AOParametersBuffer>;
    using Config = DebugAmbientOcclusionConfig;
    using JobModel = render::Job::ModelI<DebugAmbientOcclusion, Inputs, Config>;

    DebugAmbientOcclusion();

    void configure(const Config& config);
    void run(const render::RenderContextPointer& renderContext, const Inputs& inputs);
    
private:
       
    // Class describing the uniform buffer with all the parameters common to the debug AO shaders
    class Parameters {
    public:
        // Pixel info
        glm::vec4 pixelInfo { 0.0f, 0.0f, 0.0f, 0.0f };
        
        Parameters() {}
    };
    gpu::StructBuffer<Parameters> _parametersBuffer;

    const gpu::PipelinePointer& getDebugPipeline();

    gpu::PipelinePointer _debugPipeline;

    bool _showCursorPixel{ false };
};

#endif // hifi_AmbientOcclusionEffect_h
