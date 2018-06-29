#include <unistd.h> // getpagesize()
#include <boost/smart_ptr.hpp>
#include <boost/interprocess/file_mapping.hpp>
#include <boost/interprocess/mapped_region.hpp>

/*********************************************************************
 * 类功能：基于共享内存的进程间通信类，可以实例化直接使用
 * 注意：此类不绑定固定的数据结构，以字节为单位进行操作，绑定数据结构
 *           的情况使用下面的派生类struct_ipc
 *********************************************************************/
class process_memory_base
{
protected:
    boost::shared_ptr<boost::interprocess::file_mapping> _mem_file;
    //基于映射文件的共享内存池
     boost::shared_ptr<boost::interprocess::mapped_region> _mem_pool;

public:

    process_memory_base() 
    {
    }

    virtual ~process_memory_base()
    {
        close();
    }

    /*****************************************************************
     * 功能：加载共享内存
     * 说明：如果指定的映射文件已经存在，则直接打开基于该文件的共享内存
     *            如果指定的文件不存在，这创建之
     *参数：
     * @param path 字符串，输入，映射文件的全路径
     * @param bytes 无符号长整型，输入，共享内存字节尺寸
     * @param maybe_increase 布尔型，输入
     *返回值：成功返回0，失败返回－1
     ******************************************************************/
    int load(const char *path, size_t bytes, bool maybe_increase = false)
    {
        int file_handle = 0;
        off_t rounded_bytes = 0;

        if (path == 0 || strlen(path) == 0 || bytes <= 0)
            return -1;

        if (_mem_pool.get())
            close();

        if (true == maybe_increase)
        {
            file_handle = boost::interprocess::detail::create_or_open_file(path,boost::interprocess::read_write);
            if (file_handle<=0)
                return -1;

            rounded_bytes = bytes;//(bytes + ( getpagesize() - 1)) & ~( getpagesize() - 1);
            off_t file_bytes=0;
            if (!boost::interprocess::detail::get_file_size(file_handle,file_bytes))
            {
                boost::interprocess::detail::close_file(file_handle);
                return -1;
            }

            if (file_bytes < rounded_bytes)
            {
                if (!boost::interprocess::detail::truncate_file(file_handle, rounded_bytes))
                {
                    boost::interprocess::detail::close_file(file_handle);
                    return -1;
                }
                
                if (!boost::interprocess::detail::set_file_pointer(file_handle, rounded_bytes,boost::interprocess::file_begin))
                {
                    boost::interprocess::detail::close_file(file_handle);
                    return -1;
                }
            }

            boost::interprocess::detail::close_file(file_handle);
        }

        try
        {
            _mem_file.reset ( new boost::interprocess::file_mapping ( path, boost::interprocess::read_write ) );
            _mem_pool.reset ( new boost::interprocess::mapped_region ( *_mem_file, boost::interprocess::read_write ) );
            return 0;
        }
        catch ( boost::interprocess::interprocess_exception e )
        {
            fprintf ( stderr, "Error:%s\n", e.what ( ) );

        }

        return -1;
    }

    /*****************************************************************
     * 功能：关闭共享内存
     ******************************************************************/
    void close()
    {
        _mem_pool.reset();
    }

    /*****************************************************************
     * 功能：同步整个共享内存到文件系统
     ******************************************************************/
    void flush() const
    {
        if (_mem_pool.get())
        {
            _mem_pool->flush();
        }
    }

    /*****************************************************************
     * 功能：同步部分共享内存到文件系统
     *参数：
     * @param offset_addr 无符号长整型，输入，需要同步内存首地址相
     *                                  对于共享内存基地址的偏移量，从0开始
     * @param sync_bytes 无符号长整型，输入，共享内存字节尺寸
     *返回值：成功返回0，失败返回－1
     ******************************************************************/
    int flush(size_t offset_addr, size_t sync_bytes) const
    {
        if(_mem_pool->flush(offset_addr,sync_bytes))
            return 0;

        return -1;
    }

    /*****************************************************************
     * 功能：获取共享内存的基地址
     ******************************************************************/
    const void *base_addr() const
    {
        return _mem_pool->get_address();
    }

    /*****************************************************************
     * 功能：获取共享内存的字节尺寸
     ******************************************************************/
    size_t size() const
    {
        return _mem_pool->get_size();
    }

};

/*****************************************************************
 * 类功能： 基于固定数据结构的进程间内存通信模板类
 * 注意：不同于基类，所有寻址操作从1开始，0表示无效地址
 ******************************************************************/
template <class TYPE_T>
class struct_memory : public process_memory_base
{
protected:
    //共享内存的基地址
    TYPE_T *_array_ptr;
    //共享内存的容量（即容纳TYPE_T类型对象的个数）
    size_t _capacity;
public:

    struct_memory() : process_memory_base(), _array_ptr(0), _capacity()
    {
    }
    
    virtual ~struct_memory()
    {
    }

    /*****************************************************************
     * 功能：加载共享内存
     * 说明：如果指定的映射文件已经存在，则直接打开基于该文件的共享内存
     *            如果指定的文件不存在，则创建之
     *参数：
     * @param path 字符串，输入，映射文件的全路径
     * @param capacity 无符号长整型，输入，共享内存的容量（容纳TYPE_T的个数）
     * @param maybe_increase 布尔型，输入
     *返回值：成功返回0，失败返回－1
     ******************************************************************/
    int load(const char *path, size_t capacity, bool maybe_increase = false)
    {
        if (capacity <= 0)
            return -1;

        size_t bytes = capacity * sizeof (TYPE_T);
        int ret = process_memory_base::load(path, bytes, maybe_increase);
        if (0 == ret)
        {
            _array_ptr = (TYPE_T*) (_mem_pool->get_address());
            _capacity = capacity;
        }

        return ret;
    }

    /*****************************************************************
     * 功能：重载[]操作符，提供下标访问功能
     * 参数：
     *  @param position 无符号长整型，输入，共享内存中某个TYPE_T
     * 返回值：成功返回字节尺寸，失败返回0
     ******************************************************************/
    TYPE_T *operator[](size_t position) const
    {
        if (position > 0 && position <= _capacity)
            return &_array_ptr[position - 1];

        return 0;
    }

    /*****************************************************************
     * 功能：同步整个共享内存到文件系统
     ******************************************************************/
    void flush() const
    {
        process_memory_base::flush();
    }

    /*****************************************************************
     * 功能：同步单个或连续多个TYPE_T对象到文件系统
     * 参数：
     * @param position 无符号长整型，输入，共享内存中某个TYPE_T
     *              的位置，从1开始
     * @param count 无符号长整型，输入，需要同步TYPE_T对象的个数
     * 返回值：成功返回0，失败返回－1
     ******************************************************************/
    int flush(size_t position, size_t count = 1) const
    {
        if (position <= 0 || position > _capacity)
            return -1;

        if ((position + count - 1) > _capacity)
            count = _capacity - position + 1;

        return process_memory_base::flush((position - 1) * sizeof (TYPE_T), count * sizeof (TYPE_T));
    }

    /*****************************************************************
     * 功能：获取共享内存的容量（即TYPE_T对象的容纳个数）
     ******************************************************************/
    size_t capacity() const
    {
        return _capacity;
    }

};
