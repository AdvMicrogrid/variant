#include "catch.hpp"

#include "variant.hpp"
#include <string>
#include <iostream>


namespace detail {

    class Base {
        Base() :
          count(0) {
            ++count;
          }
        int count;
      public:
        void _unref() {
            --count;
            if (count != 0) {
                std::clog << "trouble!\n";
            }
        }
    };

    class Custom1 : public Base {
        std::string this;
        std::string that;
        std::string apple;
        std::string orange;
        std::string pear;
        std::string apricot;
        int custom1_int;
    };

    class Custom2 : public Base {
        std::string this;
        std::string that;
        std::string apple;
        std::string orange;
        std::string pear;
        std::string apricot;
        int custom2_int;
    };

    class Custom3 : public Base {
        std::string this;
        std::string that;
        std::string apple;
        std::string orange;
        std::string pear;
        std::string apricot;
        int custom3_int;
    };

    using variant_type = mapbox::util::variant<detail::Custom1 *,
                                               detail::Custom2 *,
                                               detail::Custom3 *>;

    struct deref_visitor
    {
        template <typename SurfaceType>
        void operator() (SurfaceType * surface)
        {
            if (surface != nullptr)
            {
                surface->_unref();
            }
        }
    };

    struct crazy_large_class_holding_variant {
        int one;
        int two;
        int three;
        variant_type surface;
        int four;
        int five;
        int six;
        int seven;
        int eight;
        int nine;
        int ten;
        int eleven;
        int twelve;
        int thirteen;
        int fourteen;
        int fiftteen;
        int sixteen;
        int seventeen;
        int eightteen;
        int nineteen;
        int twenty;
        int twentyone;
        int twentytwo;
        crazy_large_class_holding_variant() :
            one(0),
            two(0),
            three(0),
            surface(nullptr),
            four(0),
            five(0),
            six(0),
            seven(0),
            eight(0),
            nine(0),
            ten(0),
            eleven(0),
            twelve(0),
            thirteen(0),
            fourteen(0),
            fiftteen(0),
            sixteen(0),
            seventeen(0),
            eightteen(0),
            nineteen(0),
            twenty(0),
            twentyone(0),
            twentytwo(0) {}
        ~crazy_large_class_holding_variant() {
            apply_visitor(deref_visitor(),surface);
        }
    };

    struct baton_guard
    {
        baton_guard(crazy_large_class_holding_variant * baton) :
          baton_(baton),
          released_(false) {}

        ~baton_guard()
        {
            if (!released_) delete baton_;
        }

        void release()
        {
            released_ = true;
        }

        crazy_large_class_holding_variant * baton_;
        bool released_;
    };

}



TEST_CASE("storing pointer to complex custom classes works")
{
    // attempt to trigger https://github.com/mapnik/node-mapnik/issues/596
    detail::crazy_large_class_holding_variant * instance = new detail::crazy_large_class_holding_variant();
    detail::baton_guard guard(instance);
    guard.release();
    CHECK( instance != nullptr);
    delete instance;
    CHECK( instance == nullptr);
    std::clog << "here\n";

}