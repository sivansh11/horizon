#ifndef HORIZON_PIPELINE
#define HORIZON_PIPELINE

#include "horizon_core.h"

#include "horizon_device.h"

namespace horizon
{

struct PipelineConfigInfo
{
    VkViewport viewport;
    VkRect2D scissor;
    VkPipelineViewportStateCreateInfo viewportInfo;
    VkPipelineInputAssemblyStateCreateInfo inputAssemblyInfo;
    VkPipelineRasterizationStateCreateInfo rasterizationInfo;
    VkPipelineMultisampleStateCreateInfo multisampleInfo;
    VkPipelineColorBlendAttachmentState colorBlendAttachment;
    VkPipelineColorBlendStateCreateInfo colorBlendInfo;
    VkPipelineDepthStencilStateCreateInfo depthStencilInfo;
    VkPipelineLayout pipelineLayout = nullptr;
    VkRenderPass renderPass = nullptr;
    uint32_t subpass = 0;
};

class HorizonPipeline
{
public:
    HorizonPipeline(HorizonDevice &device, const std::string &vertFilePath, const std::string &fragFilePath, const PipelineConfigInfo &configInfo);
    ~HorizonPipeline();

    HorizonPipeline(const HorizonPipeline&) = delete;
    HorizonPipeline& operator=(const HorizonPipeline&) = delete;

    static PipelineConfigInfo defaultPipelineConfigInfo(uint width, uint height);

private:
    void createGraphicsPipeline(HorizonDevice &device, const std::string &vertFilePath, const std::string &fragFilePath, const PipelineConfigInfo &configInfo);
    void createShaderModule(const std::vector<char> &code, VkShaderModule *shaderModule);

private:
    HorizonDevice &horizonDevice;
    VkPipeline graphicsPipeline;
    VkShaderModule vertShaderModule;
    VkShaderModule fragShaderModule;

};

} // namespace horizon


#endif