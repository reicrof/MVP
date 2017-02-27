#ifndef VK_UTILS_H_
#define VK_UTILS_H_
#include <vulkan/vulkan.h>
#include <functional>
#include <assert.h>
#include <iostream>

#define VK_CALL( func )                                                                    \
   if ( func != VK_SUCCESS )                                                               \
   {                                                                                       \
      std::cerr << "Error when calling " << #func << " at " << __FILE__ << ":" << __LINE__ \
                << std::endl;                                                              \
      assert( false );                                                                     \
   }

template <typename T>
class VDeleter
{
  public:
   VDeleter() : VDeleter( []( T _ ) {} ) {}
   VDeleter( std::function<void( T, VkAllocationCallbacks* )> deletef )
   {
      this->deleter = [=]( T obj ) { deletef( obj, nullptr ); };
   }

   VDeleter( const VDeleter<VkInstance>& instance,
             std::function<void( VkInstance, T, VkAllocationCallbacks* )> deletef )
   {
      this->deleter = [&instance, deletef]( T obj ) { deletef( instance, obj, nullptr ); };
   }

   VDeleter( const VDeleter<VkDevice>& device,
             std::function<void( VkDevice, T, VkAllocationCallbacks* )> deletef )
   {
      this->deleter = [&device, deletef]( T obj ) { deletef( device, obj, nullptr ); };
   }

   VDeleter( VkDevice device, std::function<void( VkDevice, T, VkAllocationCallbacks* )> deletef )
   {
      this->deleter = [device, deletef]( T obj ) { deletef( device, obj, nullptr ); };
   }

   T* get() { return &object; }
   void set( const T& newObject )
   {
      cleanup();
      object = newObject;
   }

   const T* get() const { return &object; }
   const T* operator&() const { return &object; }
   ~VDeleter() { cleanup(); }
   T* operator&()
   {
      cleanup();
      return &object;
   }

   operator T() const { return object; }
  private:
   T object{VK_NULL_HANDLE};
   std::function<void( T )> deleter;

   void cleanup()
   {
      if ( object != VK_NULL_HANDLE )
      {
         deleter( object );
      }
      object = VK_NULL_HANDLE;
   }
};

#endif  // !VK_UTILS_H_
