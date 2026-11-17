#pragma once
#include <memory>
#include <utility>
#include <vector>

namespace modules::spotify
{
    // A published snapshot can share large media without copying pixels/lyrics.
    // Replacing or clearing the worker's value never mutates an older reader.
    template<class T> class shared_media_buffer
    {
        std::shared_ptr<const std::vector<T>> storage_;
    public:
        shared_media_buffer() = default;
        shared_media_buffer& operator=(std::vector<T> value)
        {
            storage_ = value.empty() ? nullptr : std::make_shared<const std::vector<T>>(std::move(value));
            return *this;
        }
        void clear() { storage_.reset(); }
        const std::vector<T>& view() const
        {
            static const std::vector<T> empty;
            return storage_ ? *storage_ : empty;
        }
        bool empty() const { return view().empty(); }
        std::size_t size() const { return view().size(); }
        const T* data() const { return view().data(); }
        const T& operator[](std::size_t index) const { return view()[index]; }
    };
}
