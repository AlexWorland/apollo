/**
 * @file src/round_robin.h
 * @brief Declarations for a round-robin iterator.
 */
#pragma once

// standard includes
#include <iterator>

/**
 * @brief A round-robin iterator utility.
 * @tparam V The value type.
 * @tparam T The iterator type.
 */
namespace round_robin_util {
  /**
   * @brief Iterator wrapper for round-robin iteration.
   * 
   * Wraps an iterator to provide round-robin iteration semantics.
   * 
   * @tparam V Value type.
   * @tparam T Iterator type.
   */
  template<class V, class T>
  class it_wrap_t {
  public:
    using iterator_category = std::random_access_iterator_tag;  ///< Iterator category.
    using value_type = V;  ///< Value type.
    using difference_type = V;  ///< Difference type.
    using pointer = V *;  ///< Pointer type.
    using const_pointer = V const *;  ///< Const pointer type.
    using reference = V &;  ///< Reference type.
    using const_reference = V const &;  ///< Const reference type.

    typedef T iterator;  ///< Iterator type.
    typedef std::ptrdiff_t diff_t;  ///< Difference type for iterator arithmetic.

    /**
     * @brief Compound addition assignment operator.
     * 
     * Advances iterator by step positions.
     * 
     * @param step Number of positions to advance.
     * @return Reference to this iterator.
     */
    iterator operator+=(diff_t step) {
      while (step-- > 0) {
        ++_this();
      }

      return _this();
    }

    /**
     * @brief Compound subtraction assignment operator.
     * 
     * Moves iterator back by step positions.
     * 
     * @param step Number of positions to move back.
     * @return Reference to this iterator.
     */
    iterator operator-=(diff_t step) {
      while (step-- > 0) {
        --_this();
      }

      return _this();
    }

    /**
     * @brief Addition operator.
     * 
     * Creates a new iterator advanced by step positions.
     * 
     * @param step Number of positions to advance.
     * @return New iterator.
     */
    iterator operator+(diff_t step) {
      iterator new_ = _this();

      return new_ += step;
    }

    /**
     * @brief Subtraction operator.
     * 
     * Creates a new iterator moved back by step positions.
     * 
     * @param step Number of positions to move back.
     * @return New iterator.
     */
    iterator operator-(diff_t step) {
      iterator new_ = _this();

      return new_ -= step;
    }

    /**
     * @brief Distance operator.
     * 
     * Calculates the distance between two iterators.
     * 
     * @param first Other iterator.
     * @return Distance between iterators.
     */
    diff_t operator-(iterator first) {
      diff_t step = 0;
      while (first != _this()) {
        ++step;
        ++first;
      }

      return step;
    }

    /**
     * @brief Pre-increment operator.
     * 
     * Advances iterator by one position.
     * 
     * @return Reference to this iterator.
     */
    iterator operator++() {
      _this().inc();
      return _this();
    }

    /**
     * @brief Pre-decrement operator.
     * 
     * Moves iterator back by one position.
     * 
     * @return Reference to this iterator.
     */
    iterator operator--() {
      _this().dec();
      return _this();
    }

    /**
     * @brief Post-increment operator.
     * 
     * Advances iterator by one position.
     * 
     * @return Copy of iterator before increment.
     */
    iterator operator++(int) {
      iterator new_ = _this();

      ++_this();

      return new_;
    }

    /**
     * @brief Post-decrement operator.
     * 
     * Moves iterator back by one position.
     * 
     * @return Copy of iterator before decrement.
     */
    iterator operator--(int) {
      iterator new_ = _this();

      --_this();

      return new_;
    }

    /**
     * @brief Dereference operator.
     * 
     * @return Reference to current element.
     */
    reference operator*() {
      return *_this().get();
    }

    /**
     * @brief Dereference operator (const).
     * 
     * @return Const reference to current element.
     */
    const_reference operator*() const {
      return *_this().get();
    }

    /**
     * @brief Member access operator.
     * 
     * @return Pointer to current element.
     */
    pointer operator->() {
      return &*_this();
    }

    /**
     * @brief Member access operator (const).
     * 
     * @return Const pointer to current element.
     */
    const_pointer operator->() const {
      return &*_this();
    }

    /**
     * @brief Inequality comparison operator.
     * 
     * @param other Other iterator to compare.
     * @return True if iterators are not equal.
     */
    bool operator!=(const iterator &other) const {
      return !(_this() == other);
    }

    /**
     * @brief Less-than comparison operator.
     * 
     * @param other Other iterator to compare.
     * @return True if this iterator is less than other.
     */
    bool operator<(const iterator &other) const {
      return !(_this() >= other);
    }

    /**
     * @brief Greater-than-or-equal comparison operator.
     * 
     * @param other Other iterator to compare.
     * @return True if this iterator is greater than or equal to other.
     */
    bool operator>=(const iterator &other) const {
      return _this() == other || _this() > other;
    }

    /**
     * @brief Less-than-or-equal comparison operator.
     * 
     * @param other Other iterator to compare.
     * @return True if this iterator is less than or equal to other.
     */
    bool operator<=(const iterator &other) const {
      return _this() == other || _this() < other;
    }

    /**
     * @brief Equality comparison operator.
     * 
     * @param other Other iterator to compare.
     * @return True if iterators are equal.
     */
    bool operator==(const iterator &other) const {
      return _this().eq(other);
    };

    /**
     * @brief Greater-than comparison operator.
     * 
     * @param other Other iterator to compare.
     * @return True if this iterator is greater than other.
     */
    bool operator>(const iterator &other) const {
      return _this().gt(other);
    }

  private:
    /**
     * @brief Get reference to derived iterator (non-const).
     * 
     * @internal Helper method for CRTP.
     * 
     * @return Reference to derived iterator.
     */
    iterator &_this() {
      return *static_cast<iterator *>(this);
    }

    /**
     * @brief Get reference to derived iterator (const).
     * 
     * @internal Helper method for CRTP.
     * 
     * @return Const reference to derived iterator.
     */
    const iterator &_this() const {
      return *static_cast<const iterator *>(this);
    }
  };

  /**
   * @brief Round-robin container iterator.
   * 
   * Provides round-robin iteration over a container, cycling through elements.
   * 
   * @tparam V Value type.
   * @tparam It Iterator type.
   */
  template<class V, class It>
  class round_robin_t: public it_wrap_t<V, round_robin_t<V, It>> {
  public:
    using iterator = It;  ///< Iterator type.
    using pointer = V *;  ///< Pointer type.

    /**
     * @brief Construct round-robin iterator.
     * 
     * @param begin Begin iterator of the range.
     * @param end End iterator of the range.
     */
    round_robin_t(iterator begin, iterator end):
        _begin(begin),
        _end(end),
        _pos(begin) {
    }

    /**
     * @brief Increment iterator (round-robin).
     * 
     * Advances to next element, wrapping to beginning if at end.
     */
    void inc() {
      ++_pos;

      if (_pos == _end) {
        _pos = _begin;
      }
    }

    /**
     * @brief Decrement iterator (round-robin).
     * 
     * Moves to previous element, wrapping to end if at beginning.
     */
    void dec() {
      if (_pos == _begin) {
        _pos = _end;
      }

      --_pos;
    }

    /**
     * @brief Check equality with another iterator.
     * 
     * Compares based on the values pointed to by iterators.
     * 
     * @param other Other iterator to compare.
     * @return True if iterators point to equal values.
     */
    bool eq(const round_robin_t &other) const {
      return *_pos == *other._pos;
    }

    /**
     * @brief Get pointer to current element.
     * 
     * @return Pointer to current element.
     */
    pointer get() const {
      return &*_pos;
    }

  private:
    It _begin;  ///< Begin iterator of the range.
    It _end;  ///< End iterator of the range.
    It _pos;  ///< Current position iterator.
  };

  /**
   * @brief Create a round-robin iterator.
   * 
   * Factory function to create a round-robin iterator over a range.
   * 
   * @tparam V Value type.
   * @tparam It Iterator type.
   * @param begin Begin iterator of the range.
   * @param end End iterator of the range.
   * @return Round-robin iterator.
   */
  template<class V, class It>
  round_robin_t<V, It> make_round_robin(It begin, It end) {
    return round_robin_t<V, It>(begin, end);
  }
}  // namespace round_robin_util
