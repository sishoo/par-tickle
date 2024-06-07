
#include <stdbool.h>

#include "bootstrap/VkBootstrap.h"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <shaderc/shaderc.h>

#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <time.h>
#include <stdlib.h>

#define VK_TRY(try)                                                      \
        do                                                               \
        {                                                                \
                if ((try) != VK_SUCCESS)                                 \
                {                                                        \
                        fprintf(stderr, #try " was not a VK_SUCCESS\n"); \
                        abort();                                         \
                }                                                        \
        } while (0);

#define MAX_MASS 1
#define SCREEN_WIDTH 800
#define SCREEN_HEIGHT 600

typedef struct
{
        float x, y;
} vector2_t;


#warning IDK WTF THIS IS
typedef struct
{
        float mass;
        vector2_t position, velocity;
} particle_t;

typedef struct
{       
        uint32_t nparticles, particles_size_bytes;
        particle_t *particles;
} particles_data_t;

static inline float rand_float(float max)
{               
        srand(time(NULL));
        return max * (rand() / (float) RAND_MAX);
}                

void particles_data_generate_random(uint32_t nparticles)
{
        uint32_t size_bytes = nparticles * sizeof(particle_t);
        
        particle_t *particles = (particle_t *)malloc(size_bytes);

        for (uint32_t i = 0; i < nparticles; i++)
        {
                particles[i] = {
                        .mass = rand_float(MAX_MASS),
                        .position = rand_float(SCREEN_WIDTH),
                        .velocity = rand_float(SCREEN_HEIGHT)
                };
        }
}

typedef struct
{               
        uint32_t __push_constant_padding;
        vector2_t position;
        vector2_t drag_direction;
} cursor_t;

typedef struct
{
        VkCommandBuffer command_buffer;
        VkFence complete_fence;
        VkSemaphore complete_semaphore;
} frame_render_infos_t;

typedef struct
{
        /* Vulkan core */
        VkInstance instance;
        VkPhysicalDevice pdevice;
        VkDevice ldevice;
        uint32_t queue_family_index;
        VkQueue queue;

        shaderc_compiler_t shader_compiler;
        uint32_t *particle_shader_source, particle_shader_size_bytes;
        VkShaderModule particle_shader_module;
        

        #warning kind of bad????
        cursor_t cursor;
        particles_data_t particles;

        uint32_t particle_buffer_swap, particle_buffer_memory_type_index;
        VkBuffer particle_buffer;
        VkDescriptorSet particle_buffer_set;
        VkDescriptorSetLayout particle_buffer_set_layout;
        VkDeviceMemory particle_buffer_memory;

        /* Pipeline */
        VkPipelineLayout particle_draw_pipeline_layout;
        VkPipeline particle_draw_pipeline;

        VkSemaphore image_acquired_semaphore;
        uint32_t nswapchain_images;
        VkSwapchainKHR swapchain;
        VkSurfaceKHR surface;
        VkImage *swapchain_images;
        uint32_t previous_resource_index;
        frame_render_infos_t *frame_render_infos;

        /* Pools */
        VkDescriptorPool descriptor_pool;
        VkCommandPool command_pool;

        bool user_input;
        uint32_t window_width, window_height;
        GLFWwindow *window;

        vkb::Instance vkb_instance;
        vkb::PhysicalDevice vkb_pdevice;
        vkb::Device vkb_ldevice;
        vkb::Swapchain vkb_swapchain;
} particle_renderer_t;

void particle_renderer_init_instance(particle_renderer_t *const r)
{
        vkb::InstanceBuilder builder;
        auto res = builder
                .set_app_name("Hephaestus Renderer")
                .set_engine_name("Hephaestus Engine")
                .require_api_version(1, 3, 0)
                .use_default_debug_messenger()
              //.set_debug_callback(debug_callback)
                .request_validation_layers()
                .build();
        r->vkb_instance = res.value();
        r->instance = r->vkb_instance.instance;
}

void particle_renderer_window_cursor_pos_callback(GLFWwindow *window, double x, double y)
{
        if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_RELEASE)
        {
                return;
        }

        #warning write code to interact with the velocity buffer/image thing
}

void particle_renderer_init_window(particle_renderer_t *const r, char *const name)
{
        glfwInit();
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        assert((r->window = glfwCreateWindow(r->window_width, r->window_height, name, NULL, NULL)));
        glfwSetCursorPosCallback(r->window, particle_renderer_window_cursor_pos_callback);
}

void particle_renderer_init_surface(particle_renderer_t *const r)
{
        VK_TRY(glfwCreateWindowSurface(r->instance, r->window, NULL, &r->surface));
}

void particle_renderer_init_pdevice(particle_renderer_t *const r)
{
        #warning DONT DELETE THIS UNTIL YOU FIX THE DAMN PICK FIRST DEVICE UNCONDITIONALLY!!!
        vkb::PhysicalDeviceSelector selector(r->vkb_instance);
        auto res = selector
                       .set_surface(r->surface)
                       .select_first_device_unconditionally()
                       .add_required_extension("VK_KHR_dynamic_rendering")
                       .select();
        r->vkb_pdevice = res.value();
        r->pdevice = r->vkb_pdevice.physical_device;
}

void particle_renderer_find_memory_type_indices(particle_renderer_t *const r)
{
        VkPhysicalDeviceMemoryProperties physical_device_memory_properties = {};
        vkGetPhysicalDeviceMemoryProperties(r->pdevice, &physical_device_memory_properties);

#if particle_renderer_GEOMETRY_BUFFER_MEMORY_TYPE_BITFLAGS == particle_renderer_REQUIRED_OBJECT_BUFFER_MEMORY_TYPE_BITFLAGS == particle_renderer_REQUIRED_DRAW_BUFFER_MEMORY_TYPE_BITFLAGS
        for (uint32_t i = 0; i < physical_device_memory_properties.memoryTypeCount; i++)
        {
                if ((particle_renderer_REQUIRED_GEOMETRY_BUFFER_BITFLAGS & physical_device_memory_properties.memoryTypes[i]) == particle_renderer_REQUIRED_GEOMETRY_BUFFER_BITFLAGS)
                {
                        r->geometry_buffer_memory_type_index = i;
                        r->object_buffer_memory_type_index = i;
                        r->draw_buffer_memory_type_index = i;

                        return;
                }
        }
#endif
}

void particle_renderer_init_ldevice(particle_renderer_t *const r)
{
        VkPhysicalDeviceSynchronization2Features sync_features = {};
        sync_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES;
        sync_features.synchronization2 = VK_TRUE;

        vkb::DeviceBuilder builder{r->vkb_pdevice};
        auto res = builder
                       .add_pNext(&sync_features)
                       .build();
        r->vkb_ldevice = res.value();
        r->ldevice = r->vkb_ldevice.device;
}

void particle_renderer_init_queue(particle_renderer_t *const r)
{
        uint32_t required = VK_QUEUE_COMPUTE_BIT | VK_QUEUE_TRANSFER_BIT, i = 0;
        for (VkQueueFamilyProperties props : r->vkb_pdevice.get_queue_families())
        {
                VkQueueFlags flags = props.queueFlags;
                if ((flags & required) == required)
                {
                        goto found_all;
                }
                i++;
        }

        found_all:
        r->queue_family_index = i;
        vkGetDeviceQueue(r->ldevice, i, 0, &r->queue);
}

void particle_renderer_init_swapchain(particle_renderer_t *const r)
{
        vkb::SwapchainBuilder builder{r->vkb_ldevice};
        auto res = builder
                       .add_image_usage_flags(VK_IMAGE_USAGE_TRANSFER_DST_BIT)
                       .build();
        r->vkb_swapchain = res.value();
        r->swapchain = r->vkb_swapchain.swapchain;
}

void particle_renderer_aquire_swapchain_images(particle_renderer_t *const r)
{
        VK_TRY(vkGetSwapchainImagesKHR(r->ldevice, r->swapchain, &r->nswapchain_images, NULL));

        r->swapchain_images = (VkImage *)calloc(sizeof(VkImage), r->nswapchain_images);

        VK_TRY(vkGetSwapchainImagesKHR(r->ldevice, r->swapchain, &r->nswapchain_images, r->swapchain_images));
}

void particle_renderer_init_command_pool(particle_renderer_t *const r)
{
        VkCommandPoolCreateInfo command_pool_create_info = {
                .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
                .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
                .queueFamilyIndex = r->queue_family_index
        };

        VK_TRY(vkCreateCommandPool(r->ldevice, &command_pool_create_info, NULL, &r->command_pool));
}

void particle_renderer_init_descriptor_pool(particle_renderer_t *const r)
{
        VkDescriptorPoolSize pool_size = {
                .type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 
                .descriptorCount = 1
        };

        VkDescriptorPoolCreateInfo pool_create_info = {
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO, 
                .flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
                .maxSets = 1,
                .poolSizeCount = 1,
                .pPoolSizes = &pool_size,
        };
                
        VK_TRY(vkCreateDescriptorPool(r->ldevice, &pool_create_info, NULL, &r->descriptor_pool));
}

void particle_renderer_init_shader_compiler(particle_renderer_t *const r)
{
        r->shader_compiler = shaderc_compiler_initialize();
}

void particle_renderer_build_particle_shader(particle_renderer_t *const r)
{
        #define PARTICLE_SHADER_PATH "shader/particle.comp"
        int fd = open("shader/particle.comp", O_RDONLY);
        assert(fd != -1);

        struct stat status;
        fstat(fd, &status);

        char *src = (char *)mmap(NULL, status.st_size, PROT_READ, MAP_PRIVATE, fd, 0);

        shaderc_compile_options_t compile_options = shaderc_compile_options_initialize();

        /* Preprocess */
        shaderc_compilation_result_t preprocess_result = 
                shaderc_compile_into_preprocessed_text(
                        r->shader_compiler, 
                        src, 
                        status.st_size, 
                        shaderc_compute_shader, 
                        "particle.comp", 
                        "main", 
                        compile_options
                );
        assert(shaderc_result_get_compilation_status(preprocess_result) == shaderc_compilation_status_success);

        /* Compile */
        shaderc_compilation_result_t compile_result = 
                shaderc_compile_into_spv(
                        r->shader_compiler,
                        shaderc_result_get_bytes(preprocess_result), 
                        shaderc_result_get_length(preprocess_result), 
                        shaderc_compute_shader, 
                        "particle.comp", 
                        "main", 
                        compile_options
                );
        assert(shaderc_result_get_compilation_status(compile_result) == shaderc_compilation_status_success);

        r->particle_shader_source = (uint32_t *)shaderc_result_get_bytes(compile_result);
        r->particle_shader_size_bytes = shaderc_result_get_length(compile_result);
}

void particle_renderer_build_pipelines(particle_renderer_t *const r)
{
        /* Particle draw pipeline */ 
        #warning what is up with the add 4???? probably adding padding for the vec2 thing?
        VkPushConstantRange push_constant_range = {     
                .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
                .size = sizeof(r->cursor) + 4
        };

        VkPipelineLayoutCreateInfo pipeline_layout_create_info = {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
                .setLayoutCount = 1,
                .pSetLayouts = &r->particle_buffer_set_layout,
                .pushConstantRangeCount = 1,
                .pPushConstantRanges = &push_constant_range
        };

        VK_TRY(vkCreatePipelineLayout(r->ldevice, &pipeline_layout_create_info, NULL, &r->particle_draw_pipeline_layout));
        
        VkShaderModuleCreateInfo shader_module_create_info = {
                .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
                .codeSize = r->particle_shader_size_bytes,
                .pCode = r->particle_shader_source
        };

        VK_TRY(vkCreateShaderModule(r->ldevice, &shader_module_create_info, NULL, &r->particle_shader_module));

        VkPipelineShaderStageCreateInfo shader_stage_create_info = {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .stage = VK_SHADER_STAGE_COMPUTE_BIT,
                .module = r->particle_shader_module,
                .pName = "main"
        };

        VkComputePipelineCreateInfo particle_draw_pipeline_create_info = {
                .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
                .stage = shader_stage_create_info,
                .layout = r->particle_draw_pipeline_layout
        };

        VK_TRY(vkCreateComputePipelines(r->ldevice, VK_NULL_HANDLE, 1, &particle_draw_pipeline_create_info, NULL, &r->particle_draw_pipeline));
}

void particle_renderer_init_particle_buffer(particle_renderer_t *const r)
{

        #warning add the spacial grid do the radius thing 

        uint32_t size = r->particles.nparticles * sizeof(particle_t) * 2;  //  x2 because double buffer

        /* Allocate buffer memory */
        VkMemoryAllocateInfo buffer_allocate_info = {
                .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,                
                .allocationSize = size,
                .memoryTypeIndex = r->particle_buffer_memory_type_index
        };

        VK_TRY(vkAllocateMemory(r->ldevice, &buffer_allocate_info, NULL, &r->particle_buffer_memory));

        /* Init buffer object */
        VkBufferCreateInfo buffer_create_info = {
                .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                .size = size,
                .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
                .queueFamilyIndexCount = 1,
                .pQueueFamilyIndices = &r->queue_family_index
        };

        VK_TRY(vkCreateBuffer(r->ldevice, &buffer_create_info, NULL, &r->particle_buffer));

        VK_TRY(vkBindBufferMemory(r->ldevice, r->particle_buffer, r->particle_buffer_memory, 0));
        
        /* Init descriptor set */
        VkDescriptorSetLayoutBinding set_layout_binding = {
                .binding = 0,
                .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                .descriptorCount = 1,
                .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT
        };

        VkDescriptorSetLayoutCreateInfo set_layout_create_info = {    
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
                .bindingCount = 1,
                .pBindings = &set_layout_binding
        };

        VK_TRY(vkCreateDescriptorSetLayout(r->ldevice, &set_layout_create_info, NULL, &r->particle_buffer_set_layout));
 
        VkDescriptorSetAllocateInfo set_allocate_info = {
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
                .descriptorPool = r->descriptor_pool,
                .descriptorSetCount = 1,
                .pSetLayouts = &r->particle_buffer_set_layout
        };

        VK_TRY(vkAllocateDescriptorSets(r->ldevice, &set_allocate_info, &r->particle_buffer_set));
}

void particle_renderer_init_frame_render_infos(particle_renderer_t *const r)
{
        VkFenceCreateInfo fence_create_info = {
                .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
                .flags = VK_FENCE_CREATE_SIGNALED_BIT
        };

        VkSemaphoreCreateInfo semaphore_create_info = {
                .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
        };

        VkCommandBufferAllocateInfo command_buffer_allocate_info = {
                .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
                .commandPool = r->command_pool,
                .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
                .commandBufferCount = 1
        };

        r->frame_render_infos = (frame_render_infos_t *)malloc(sizeof(frame_render_infos_t) * r->nswapchain_images);

        for (uint32_t i = 0; i < r->nswapchain_images; i++)
        {
                VK_TRY(vkCreateFence(r->ldevice, &fence_create_info, NULL, &r->frame_render_infos[i].complete_fence));
                VK_TRY(vkCreateSemaphore(r->ldevice, &semaphore_create_info, NULL, &r->frame_render_infos[i].complete_semaphore));
                VK_TRY(vkAllocateCommandBuffers(r->ldevice, &command_buffer_allocate_info, &r->frame_render_infos[i].command_buffer));
        }

        VK_TRY(vkCreateSemaphore(r->ldevice, &semaphore_create_info, NULL, &r->image_acquired_semaphore));
}

void particles_data_init(particles_data_t *const p)
{       
        p->nparticles = 100;
}

void particle_renderer_init(particle_renderer_t *const r, char *const window_name, int width, int height)
{
        particles_data_init(&r->particles);

        /* Vulkan Core */
        particle_renderer_init_instance(r);
        r->window_width = width;
        r->window_height = height;
        particle_renderer_init_window(r, window_name);
        particle_renderer_init_surface(r);
        particle_renderer_init_pdevice(r);
        particle_renderer_init_ldevice(r);
        particle_renderer_init_queue(r);
        particle_renderer_init_swapchain(r);
        particle_renderer_aquire_swapchain_images(r);
        particle_renderer_init_command_pool(r);
        particle_renderer_init_descriptor_pool(r);
        particle_renderer_init_shader_compiler(r);

        /* Particle Specifics */
        particle_renderer_init_frame_render_infos(r);
        particle_renderer_find_memory_type_indices(r);
        particle_renderer_init_particle_buffer(r);
        particle_renderer_build_particle_shader(r);
        particle_renderer_build_pipelines(r);
}

void particle_renderer_upload_particle_data(particle_renderer_t *const r)
{
        void *ptr;
        VK_TRY(vkMapMemory(r->ldevice, r->particle_buffer_memory, 0, r->particle_buffer_memory_size, , &ptr));
}

void particle_renderer_draw(particle_renderer_t *const r)
{
        uint32_t resource_index = r->previous_resource_index + 1;
        if (resource_index == r->nswapchain_images)
        {
                resource_index = 0;
        }

        frame_render_infos_t frame_infos = r->frame_render_infos[resource_index];

        VK_TRY(vkWaitForFences(r->ldevice, 1, &r->frame_render_infos[r->previous_resource_index].complete_fence, VK_TRUE, 10));
        VK_TRY(vkResetFences(r->ldevice, 1, &r->frame_render_infos[r->previous_resource_index].complete_fence));

        uint32_t image_index;
        VK_TRY(vkAcquireNextImageKHR(r->ldevice, r->swapchain, 10, r->image_acquired_semaphore, VK_NULL_HANDLE, &image_index));
        VkImage target_image = r->swapchain_images[image_index];


        /* Record frame's commands */
        vkResetCommandBuffer(frame_infos.command_buffer, 0);

        VkCommandBufferBeginInfo command_buffer_begin_info = {
                .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
        };

        vkBeginCommandBuffer(frame_infos.command_buffer, &command_buffer_begin_info);

        r->cursor.__push_constant_padding = r->user_input;
        vkCmdPushConstants(
                frame_infos.command_buffer,
                r->particle_draw_pipeline_layout,
                VK_SHADER_STAGE_COMPUTE_BIT,
                0,
                sizeof(r->cursor) + 4,
                &r->cursor.__push_constant_padding
        );
        
        /* Particle update */
        uint32_t work_group_size = ceil(r->particles.nparticles / 16);

        
        vkCmdBindDescriptorSets(
                frame_infos.command_buffer,
                VK_PIPELINE_BIND_POINT_COMPUTE,
                r->particle_draw_pipeline_layout,
                0,
                1,
                &r->particle_buffer_set,
                0,
                NULL
        ); 

        vkCmdBindPipeline(frame_infos.command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, r->particle_draw_pipeline);  


        vkCmdDispatch(frame_infos.command_buffer, work_group_size, work_group_size, 1);


        // VkImageSubresourceLayers target_image_subresource_layers = {
        //         .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        //         .layerCount = 1
        // };

        // VkBufferImageCopy2 regions = {
        //         .bufferOffset = 0,
        //         .bufferRowLength = r->window_width,
        //         .bufferImageHeight = r->window_height,
        //         .imageSubresource = target_image_subresource_layers,
        //         .imageOffset = {0, 0, 0},
        //         .imageExtent = {r->window_width, r->window_height, 1}
        // };

        // VkCopyBufferToImageInfo2 copy_info = {
        //         .sType = VK_STRUCTURE_TYPE_COPY_BUFFER_TO_IMAGE_INFO_2,
        //         .srcBuffer = r->particle_buffer,
        //         .dstImage = target_image,
        //         .dstImageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 
        //         .regionCount = 1,
        //         .pRegions = &regions
        // };

        // vkCmdCopyBufferToImage2(frame_infos.command_buffer, &copy_info);

        VkImageMemoryBarrier2 target_image_translate_2_memory_barrier = {
                .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                .oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                .newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
        };

        VkDependencyInfo translate_2_dependency_info = {
                .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                .imageMemoryBarrierCount = 1,
                .pImageMemoryBarriers = &target_image_translate_2_memory_barrier
        };

        vkCmdPipelineBarrier2(frame_infos.command_buffer, &translate_2_dependency_info);

        vkEndCommandBuffer(frame_infos.command_buffer); 

        VkSubmitInfo submit_info = {
                .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                .waitSemaphoreCount = 1,
                .pWaitSemaphores = &r->image_acquired_semaphore,
                .pCommandBuffers = &frame_infos.command_buffer
        };

        VK_TRY(vkQueueSubmit(r->queue, 1, &submit_info, frame_infos.complete_fence));

        VkPresentInfoKHR present_info = {
                .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
                .waitSemaphoreCount = 1,
                .pWaitSemaphores = &r->image_acquired_semaphore,
                .swapchainCount = 1,
                .pSwapchains = &r->swapchain,
                .pImageIndices = &image_index,
        };      

        VK_TRY(vkQueuePresentKHR(r->queue, &present_info));

        r->previous_resource_index = resource_index;
}

int main()
{       
        particles_data_t particles = {};
        particles_data_generate_random(100);

        particle_renderer_t renderer = {};
        particle_renderer_init(&renderer, "Particle", 800, 600);

        particle_renderer_upload_particle_data(renderer);

        uint64_t frame_num = 0;
        while (!glfwWindowShouldClose(renderer.window))
        {
                printf("Frame Num: %llu\n", frame_num);

                // if (!glfwGetWindowAttrib(particles.window, GLFW_FOCUSED))
                // {       
                //         nanosleep((struct timespec[]){{.tv_nsec = 5000}}, NULL);
                //         continue;
                // }

                particle_renderer_draw(&renderer);

                renderer.particle_buffer_swap = !renderer.particle_buffer_swap;
                renderer.user_input = 0;

                __builtin_trap();
        }

        glfwTerminate();

        return 0;
}