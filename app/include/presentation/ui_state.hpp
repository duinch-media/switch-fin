#pragma once

#include <string>
#include <vector>
#include "domain/models/media_item.hpp"

namespace presentation {

enum class UiStatus {
    IDLE = 0,
    LOADING,
    SUCCESS,
    EMPTY,
    ERROR,
};

struct UiState {
    UiStatus status = UiStatus::IDLE;
    std::string errorMessage;
    std::vector<domain::MediaItemEntity> items;

    bool isLoading() const noexcept { return status == UiStatus::LOADING; }
    bool isSuccess() const noexcept { return status == UiStatus::SUCCESS; }
    bool isEmpty() const noexcept { return status == UiStatus::EMPTY; }
    bool isError() const noexcept { return status == UiStatus::ERROR; }

    static UiState loading() {
        UiState s;
        s.status = UiStatus::LOADING;
        return s;
    }

    static UiState success(std::vector<domain::MediaItemEntity> items) {
        UiState s;
        s.items = std::move(items);
        s.status = s.items.empty() ? UiStatus::EMPTY : UiStatus::SUCCESS;
        return s;
    }

    static UiState error(std::string message) {
        UiState s;
        s.status = UiStatus::ERROR;
        s.errorMessage = std::move(message);
        return s;
    }
};

} // namespace presentation
