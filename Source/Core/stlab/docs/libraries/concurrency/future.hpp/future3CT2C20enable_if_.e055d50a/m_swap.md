---
layout: method
title: swap
owner: __MISSING__
brief: __MISSING__
tags:
  - method
defined_in_file: concurrency/future.hpp
overloads:
  void swap(future<T, typename enable_if<smart_is_copy_constructible_v<T>, void>::type> &):
    arguments:
      - description: __OPTIONAL__
        name: x
        type: future<T, typename enable_if<smart_is_copy_constructible_v<T>, void>::type> &
    description: __OPTIONAL__
    return: __OPTIONAL__
    signature_with_names: void swap(future<T, typename enable_if<smart_is_copy_constructible_v<T>, void>::type> & x)
---