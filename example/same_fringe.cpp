
//          Copyright Nat Goodspeed 2013.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSEstd::placeholders::_1_0.txt or copy at
//          http://www.boost.org/LICENSEstd::placeholders::_1_0.txt)

// Adapted example from Boost.Coroutine - Jamboree 2015

#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <iterator>
#include <string>
#include <utility>

#include <co2/recursive_generator.hpp>
#include <co2/utility/stack_allocator.hpp>

struct node
{
    typedef std::shared_ptr< node >    ptr_t;

    // Each tree node has an optional left subtree, an optional right subtree
    // and a value of its own. The value is considered to be between the left
    // subtree and the right.
    ptr_t left, right;
    std::string value;

    // construct leaf
    node(const std::string& v):
        left(),right(),value(v)
    {}
    // construct nonleaf
    node(ptr_t l, const std::string& v, ptr_t r):
        left(l),right(r),value(v)
    {}

    static ptr_t create(const std::string& v)
    {
        return ptr_t(new node(v));
    }

    static ptr_t create(ptr_t l, const std::string& v, ptr_t r)
    {
        return ptr_t(new node(l, v, r));
    }
};

node::ptr_t create_left_tree_from(const std::string& root)
{
    /* --------
         root
         / \
        b   e
       / \
      a   c
     -------- */

    return node::create(
            node::create(
                node::create("a"),
                "b",
                node::create("c")),
            root,
            node::create("e"));
}

node::ptr_t create_right_tree_from(const std::string& root)
{
    /* --------
         root
         / \
        a   d
           / \
          c   e
       -------- */

    return node::create(
            node::create("a"),
            root,
            node::create(
                node::create("c"),
                "d",
                node::create("e")));
}

// recursively walk the tree, delivering values in order
template<class Alloc>
auto traverse(Alloc alloc, node::ptr_t n)
CO2_BEG(co2::recursive_generator<std::string>, (alloc, n)new(alloc))
{
    if (n->left)
        CO2_YIELD(traverse(alloc, n->left));
    CO2_YIELD(n->value);
    if (n->right)
        CO2_YIELD(traverse(alloc, n->right));
} CO2_END

int main()
{
    co2::stack_buffer<2 * 1024> buf;
    co2::stack_allocator<> alloc(buf);
    {
        node::ptr_t left_d(create_left_tree_from("d"));
        auto left_d_reader(traverse(alloc, left_d));
        std::cout << "left tree from d:\n";
        std::copy(std::begin(left_d_reader),
                  std::end(left_d_reader),
                  std::ostream_iterator<std::string>(std::cout, " "));
        std::cout << std::endl;

        node::ptr_t right_b(create_right_tree_from("b"));
        auto right_b_reader(traverse(alloc, right_b));
        std::cout << "right tree from b:\n";
        std::copy(std::begin(right_b_reader),
                  std::end(right_b_reader),
                  std::ostream_iterator<std::string>(std::cout, " "));
        std::cout << std::endl;

        node::ptr_t right_x(create_right_tree_from("x"));
        auto right_x_reader(traverse(alloc, right_x));
        std::cout << "right tree from x:\n";
        std::copy(std::begin(right_x_reader),
                  std::end(right_x_reader),
                  std::ostream_iterator<std::string>(std::cout, " "));
        std::cout << std::endl;
    }
    buf.clear();
    {
        node::ptr_t left_d(create_left_tree_from("d"));
        auto left_d_reader(traverse(alloc, left_d));

        node::ptr_t right_b(create_right_tree_from("b"));
        auto right_b_reader(traverse(alloc, right_b));

        std::cout << "left tree from d == right tree from b? "
                  << std::boolalpha
                  << std::equal(std::begin(left_d_reader),
                                std::end(left_d_reader),
                                std::begin(right_b_reader))
                  << std::endl;
    }
    buf.clear();
    {
        node::ptr_t left_d(create_left_tree_from("d"));
        auto left_d_reader(traverse(alloc, left_d));

        node::ptr_t right_x(create_right_tree_from("x"));
        auto right_x_reader(traverse(alloc, right_x));

        std::cout << "left tree from d == right tree from x? "
                  << std::boolalpha
                  << std::equal(std::begin(left_d_reader),
                                std::end(left_d_reader),
                                std::begin(right_x_reader))
                  << std::endl;
    }
    buf.clear();
    std::cout << "Done" << std::endl;

    return EXIT_SUCCESS;
}
