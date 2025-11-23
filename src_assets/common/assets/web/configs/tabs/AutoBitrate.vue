<script setup>
import {ref, inject} from 'vue'

const $t = inject('i18n').t;

const props = defineProps([
  'platform',
  'config',
])

const config = ref(props.config)

const validateMinBitrate = (event) => {
  const value = parseInt(event.target.value);
  if (isNaN(value) || value < 500) {
    event.target.setCustomValidity($t('config.auto_bitrate_min_kbps_error'));
  } else {
    event.target.setCustomValidity('');
  }
  event.target.reportValidity();
}

const validateMaxBitrate = (event) => {
  const value = parseInt(event.target.value);
  const minValue = parseInt(config.value.auto_bitrate_min_kbps) || 500;
  if (value !== 0 && (isNaN(value) || value < minValue)) {
    event.target.setCustomValidity($t('config.auto_bitrate_max_kbps_error'));
  } else {
    event.target.setCustomValidity('');
  }
  event.target.reportValidity();
}

const validateAdjustmentInterval = (event) => {
  const value = parseInt(event.target.value);
  if (isNaN(value) || value < 1000) {
    event.target.setCustomValidity($t('config.auto_bitrate_adjustment_interval_ms_error'));
  } else {
    event.target.setCustomValidity('');
  }
  event.target.reportValidity();
}
</script>

<template>
  <div id="auto-bitrate" class="config-page">
    <!-- Minimum Bitrate -->
    <div class="mb-3">
      <label for="auto_bitrate_min_kbps" class="form-label">{{ $t('config.auto_bitrate_min_kbps') }}</label>
      <input
        type="number"
        class="form-control"
        id="auto_bitrate_min_kbps"
        v-model.number="config.auto_bitrate_min_kbps"
        min="500"
        step="100"
        @input="validateMinBitrate"
      />
      <div class="form-text">{{ $t('config.auto_bitrate_min_kbps_desc') }}</div>
    </div>

    <!-- Maximum Bitrate -->
    <div class="mb-3">
      <label for="auto_bitrate_max_kbps" class="form-label">{{ $t('config.auto_bitrate_max_kbps') }}</label>
      <input
        type="number"
        class="form-control"
        id="auto_bitrate_max_kbps"
        v-model.number="config.auto_bitrate_max_kbps"
        min="0"
        step="1000"
        @input="validateMaxBitrate"
      />
      <div class="form-text">{{ $t('config.auto_bitrate_max_kbps_desc') }}</div>
    </div>

    <!-- Adjustment Interval -->
    <div class="mb-3">
      <label for="auto_bitrate_adjustment_interval_ms" class="form-label">{{ $t('config.auto_bitrate_adjustment_interval_ms') }}</label>
      <input
        type="number"
        class="form-control"
        id="auto_bitrate_adjustment_interval_ms"
        v-model.number="config.auto_bitrate_adjustment_interval_ms"
        min="1000"
        step="500"
        @input="validateAdjustmentInterval"
      />
      <div class="form-text">{{ $t('config.auto_bitrate_adjustment_interval_ms_desc') }}</div>
    </div>

    <div class="alert alert-info">
      <i class="fa-solid fa-xl fa-circle-info"></i>
      {{ $t('config.auto_bitrate_info') }}
    </div>
  </div>
</template>

<style scoped>
</style>

