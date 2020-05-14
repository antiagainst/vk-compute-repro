// This is free and unencumbered software released into the public domain.
//
// Anyone is free to copy, modify, publish, use, compile, sell, or
// distribute this software, either in source code form or as a compiled
// binary, for any purpose, commercial or non-commercial, and by any
// means.
//
// In jurisdictions that recognize copyright laws, the author or authors
// of this software dedicate any and all copyright interest in the
// software to the public domain. We make this dedication for the benefit
// of the public at large and to the detriment of our heirs and
// successors. We intend this dedication to be an overt act of
// relinquishment in perpetuity of all present and future rights to this
// software under copyright law.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
// EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
// IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
// OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
// ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
// OTHER DEALINGS IN THE SOFTWARE.
//
// For more information, please refer to <http://unlicense.org/>

#include <fstream>
#include <ios>
#include <iostream>
#include <iterator>
#include <stdio.h>
#include <stdlib.h>
#include <vector>
#include <vulkan/vulkan.h>

#define BAIL_ON_BAD_RESULT(result)                                             \
  if (VK_SUCCESS != (result)) {                                                \
    fprintf(stderr, "Failure at %u %s with error code: %d\n", __LINE__,        \
            __FILE__, result);                                                 \
    exit(-1);                                                                  \
  }

VkResult vkGetBestComputeQueueNPH(VkPhysicalDevice physicalDevice,
                                  uint32_t *queueFamilyIndex) {
  uint32_t queueFamilyPropertiesCount = 0;
  vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice,
                                           &queueFamilyPropertiesCount, 0);

  std::vector<VkQueueFamilyProperties> queueFamilyProperties(
      queueFamilyPropertiesCount);

  vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice,
                                           &queueFamilyPropertiesCount,
                                           queueFamilyProperties.data());

  // first try and find a queue that has just the compute bit set
  for (uint32_t i = 0; i < queueFamilyPropertiesCount; i++) {
    // mask out the sparse binding bit that we aren't caring about (yet!) and
    // the transfer bit
    const VkQueueFlags maskedFlags =
        (~(VK_QUEUE_TRANSFER_BIT | VK_QUEUE_SPARSE_BINDING_BIT) &
         queueFamilyProperties[i].queueFlags);

    if (!(VK_QUEUE_GRAPHICS_BIT & maskedFlags) &&
        (VK_QUEUE_COMPUTE_BIT & maskedFlags)) {
      *queueFamilyIndex = i;
      return VK_SUCCESS;
    }
  }

  // lastly get any queue that'll work for us
  for (uint32_t i = 0; i < queueFamilyPropertiesCount; i++) {
    // mask out the sparse binding bit that we aren't caring about (yet!) and
    // the transfer bit
    const VkQueueFlags maskedFlags =
        (~(VK_QUEUE_TRANSFER_BIT | VK_QUEUE_SPARSE_BINDING_BIT) &
         queueFamilyProperties[i].queueFlags);

    if (VK_QUEUE_COMPUTE_BIT & maskedFlags) {
      *queueFamilyIndex = i;
      return VK_SUCCESS;
    }
  }

  return VK_ERROR_INITIALIZATION_FAILED;
}

int main(int argc, const char *const argv[]) {
  if (argc != 3) {
    fprintf(stderr, "usage: repro <shader-file-name> <entry-point-name>\n");
    return 1;
  }

  const VkApplicationInfo applicationInfo = {VK_STRUCTURE_TYPE_APPLICATION_INFO,
                                             0,
                                             "VKComputeSample",
                                             0,
                                             "",
                                             0,
                                             VK_MAKE_VERSION(1, 0, 9)};

  const VkInstanceCreateInfo instanceCreateInfo = {
      VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
      0,
      0,
      &applicationInfo,
      0,
      0,
      0,
      0};

  VkInstance instance;
  BAIL_ON_BAD_RESULT(vkCreateInstance(&instanceCreateInfo, 0, &instance));

  uint32_t physicalDeviceCount = 0;
  BAIL_ON_BAD_RESULT(
      vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, 0));

  std::vector<VkPhysicalDevice> physicalDevices(physicalDeviceCount);

  BAIL_ON_BAD_RESULT(vkEnumeratePhysicalDevices(instance, &physicalDeviceCount,
                                                physicalDevices.data()));

  for (uint32_t i = 0; i < physicalDeviceCount; i++) {
    uint32_t queueFamilyIndex = 0;
    BAIL_ON_BAD_RESULT(
        vkGetBestComputeQueueNPH(physicalDevices[i], &queueFamilyIndex));

    const float queuePrioritory = 1.0f;
    const VkDeviceQueueCreateInfo deviceQueueCreateInfo = {
        VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        0,
        0,
        queueFamilyIndex,
        1,
        &queuePrioritory};

    const VkDeviceCreateInfo deviceCreateInfo = {
        VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        0,
        0,
        1,
        &deviceQueueCreateInfo,
        0,
        0,
        0,
        0,
        0};

    VkDevice device;
    BAIL_ON_BAD_RESULT(
        vkCreateDevice(physicalDevices[i], &deviceCreateInfo, 0, &device));

    VkPhysicalDeviceMemoryProperties properties;

    vkGetPhysicalDeviceMemoryProperties(physicalDevices[i], &properties);

    const int32_t outBufferCount = 20;

    const uint32_t in1BufferSize = sizeof(float) * 20;
    const uint32_t in2BufferSize = sizeof(float) * 6;
    const uint32_t outBufferSize = sizeof(float) * outBufferCount;

    // set memoryTypeIndex to an invalid entry in the properties.memoryTypes
    // array
    uint32_t memoryTypeIndex = VK_MAX_MEMORY_TYPES;

    for (uint32_t k = 0; k < properties.memoryTypeCount; k++) {
      if ((VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT &
           properties.memoryTypes[k].propertyFlags) &&
          (VK_MEMORY_PROPERTY_HOST_COHERENT_BIT &
           properties.memoryTypes[k].propertyFlags) &&
          (in1BufferSize + in2BufferSize + outBufferSize <
           properties.memoryHeaps[properties.memoryTypes[k].heapIndex].size)) {
        memoryTypeIndex = k;
        break;
      }
    }

    BAIL_ON_BAD_RESULT(memoryTypeIndex == VK_MAX_MEMORY_TYPES
                           ? VK_ERROR_OUT_OF_HOST_MEMORY
                           : VK_SUCCESS);

    const VkMemoryAllocateInfo in1MemInfo = {
        VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, 0, in1BufferSize,
        memoryTypeIndex};
    const VkMemoryAllocateInfo in2MemInfo = {
        VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, 0, in2BufferSize,
        memoryTypeIndex};
    const VkMemoryAllocateInfo outMemInfo = {
        VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, 0, outBufferSize,
        memoryTypeIndex};

    VkDeviceMemory in1Memory, in2Memory, outMemory;
    BAIL_ON_BAD_RESULT(vkAllocateMemory(device, &in1MemInfo, 0, &in1Memory));
    BAIL_ON_BAD_RESULT(vkAllocateMemory(device, &in2MemInfo, 0, &in2Memory));
    BAIL_ON_BAD_RESULT(vkAllocateMemory(device, &outMemInfo, 0, &outMemory));

    float *inputs;
    BAIL_ON_BAD_RESULT(
        vkMapMemory(device, in1Memory, 0, in1BufferSize, 0, (void **)&inputs));
    for (uint32_t k = 0; k < 20; k++) {
      inputs[k] = k;
    }
    vkUnmapMemory(device, in1Memory);

    BAIL_ON_BAD_RESULT(
        vkMapMemory(device, in2Memory, 0, in2BufferSize, 0, (void **)&inputs));
    inputs[0] = 1.;
    inputs[1] = 4.;
    inputs[2] = 2.;
    inputs[3] = -2.;
    inputs[4] = 0.;
    inputs[5] = 1.;

    vkUnmapMemory(device, in2Memory);

    const VkBufferCreateInfo in1BufferCreateInfo = {
        VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        0,
        0,
        in1BufferSize,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_SHARING_MODE_EXCLUSIVE,
        1,
        &queueFamilyIndex};

    VkBuffer in1_buffer;
    BAIL_ON_BAD_RESULT(
        vkCreateBuffer(device, &in1BufferCreateInfo, 0, &in1_buffer));

    BAIL_ON_BAD_RESULT(vkBindBufferMemory(device, in1_buffer, in1Memory, 0));

    const VkBufferCreateInfo in2BufferCreateInfo = {
        VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        0,
        0,
        in2BufferSize,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_SHARING_MODE_EXCLUSIVE,
        1,
        &queueFamilyIndex};

    VkBuffer in2_buffer;
    BAIL_ON_BAD_RESULT(
        vkCreateBuffer(device, &in2BufferCreateInfo, 0, &in2_buffer));

    BAIL_ON_BAD_RESULT(vkBindBufferMemory(device, in2_buffer, in2Memory, 0));

    const VkBufferCreateInfo outBufferCreateInfo = {
        VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        0,
        0,
        outBufferSize,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_SHARING_MODE_EXCLUSIVE,
        1,
        &queueFamilyIndex};

    VkBuffer out_buffer;
    BAIL_ON_BAD_RESULT(
        vkCreateBuffer(device, &outBufferCreateInfo, 0, &out_buffer));

    BAIL_ON_BAD_RESULT(
        vkBindBufferMemory(device, out_buffer, outMemory, 0));

    const char *shaderFileName = argv[1];
    std::ifstream shaderFile(shaderFileName, std::ios::binary);
    std::vector<char> shader(std::istreambuf_iterator<char>(shaderFile), {});

    VkShaderModuleCreateInfo shaderModuleCreateInfo = {
        VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO, 0, 0, shader.size(),
        reinterpret_cast<const uint32_t *>(shader.data())};

    VkShaderModule shader_module;

    BAIL_ON_BAD_RESULT(vkCreateShaderModule(device, &shaderModuleCreateInfo, 0,
                                            &shader_module));

    VkDescriptorSetLayoutBinding descriptorSetLayoutBindings[] = {
        {0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT,
         0},
        {1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT,
         0},
        {2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT,
         0}};

    VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo = {
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, 0, 0, 3,
        descriptorSetLayoutBindings};

    VkDescriptorSetLayout descriptorSetLayout;
    BAIL_ON_BAD_RESULT(vkCreateDescriptorSetLayout(
        device, &descriptorSetLayoutCreateInfo, 0, &descriptorSetLayout));

    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        0,
        0,
        1,
        &descriptorSetLayout,
        0,
        0};

    VkPipelineLayout pipelineLayout;
    BAIL_ON_BAD_RESULT(vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo,
                                              0, &pipelineLayout));

    const char *entryPointName = argv[2];
    VkComputePipelineCreateInfo computePipelineCreateInfo = {
        VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        0,
        0,
        {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, 0, 0,
         VK_SHADER_STAGE_COMPUTE_BIT, shader_module, entryPointName, 0},
        pipelineLayout,
        0,
        0};

    VkPipeline pipeline;
    BAIL_ON_BAD_RESULT(vkCreateComputePipelines(
        device, 0, 1, &computePipelineCreateInfo, 0, &pipeline));

    VkCommandPoolCreateInfo commandPoolCreateInfo = {
        VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO, 0, 0, queueFamilyIndex};

    VkDescriptorPoolSize descriptorPoolSize = {
        VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 3};

    VkDescriptorPoolCreateInfo descriptorPoolCreateInfo = {
        VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        0,
        0,
        1,
        1,
        &descriptorPoolSize};

    VkDescriptorPool descriptorPool;
    BAIL_ON_BAD_RESULT(vkCreateDescriptorPool(device, &descriptorPoolCreateInfo,
                                              0, &descriptorPool));

    VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = {
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, 0, descriptorPool, 1,
        &descriptorSetLayout};

    VkDescriptorSet descriptorSet;
    BAIL_ON_BAD_RESULT(vkAllocateDescriptorSets(
        device, &descriptorSetAllocateInfo, &descriptorSet));

    VkDescriptorBufferInfo in1BufferInfo = {in1_buffer, 0, VK_WHOLE_SIZE};
    VkDescriptorBufferInfo in2BufferInfo = {in2_buffer, 0, VK_WHOLE_SIZE};
    VkDescriptorBufferInfo out_descriptorBufferInfo = {out_buffer, 0,
                                                       VK_WHOLE_SIZE};

    VkWriteDescriptorSet writeDescriptorSet[] = {
        {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, 0, descriptorSet, 0, 0, 1,
         VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 0, &in1BufferInfo, 0},
        {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, 0, descriptorSet, 1, 0, 1,
         VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 0, &in2BufferInfo, 0},
        {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, 0, descriptorSet, 2, 0, 1,
         VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 0, &out_descriptorBufferInfo, 0}};

    vkUpdateDescriptorSets(device, 3, writeDescriptorSet, 0, 0);

    VkCommandPool commandPool;
    BAIL_ON_BAD_RESULT(
        vkCreateCommandPool(device, &commandPoolCreateInfo, 0, &commandPool));

    VkCommandBufferAllocateInfo commandBufferAllocateInfo = {
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, 0, commandPool,
        VK_COMMAND_BUFFER_LEVEL_PRIMARY, 1};

    VkCommandBuffer commandBuffer;
    BAIL_ON_BAD_RESULT(vkAllocateCommandBuffers(
        device, &commandBufferAllocateInfo, &commandBuffer));

    VkCommandBufferBeginInfo commandBufferBeginInfo = {
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, 0,
        VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT, 0};

    BAIL_ON_BAD_RESULT(
        vkBeginCommandBuffer(commandBuffer, &commandBufferBeginInfo));

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);

    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                            pipelineLayout, 0, 1, &descriptorSet, 0, 0);

    vkCmdDispatch(commandBuffer, 1, 1, 1);

    BAIL_ON_BAD_RESULT(vkEndCommandBuffer(commandBuffer));

    VkQueue queue;
    vkGetDeviceQueue(device, queueFamilyIndex, 0, &queue);

    VkSubmitInfo submitInfo = {
        VK_STRUCTURE_TYPE_SUBMIT_INFO, 0, 0, 0, 0, 1, &commandBuffer, 0, 0};

    BAIL_ON_BAD_RESULT(vkQueueSubmit(queue, 1, &submitInfo, 0));

    BAIL_ON_BAD_RESULT(vkQueueWaitIdle(queue));

    float *outputs;
    BAIL_ON_BAD_RESULT(
        vkMapMemory(device, outMemory, 0, outBufferSize, 0, (void **)&outputs));

    for (uint32_t k = 0; k < outBufferCount; ++k) {
      fprintf(stdout, " %f", outputs[k]);
    }
    fprintf(stdout, "\n");
  }
}
