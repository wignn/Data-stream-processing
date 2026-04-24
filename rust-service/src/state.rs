pub struct ProcessingState {
    cvd: f64,
    buy_volume: f64,
    sell_volume: f64,
    tick_count: u64,
}

impl ProcessingState {
    pub fn new() -> Self {
        Self {
            cvd: 0.0,
            buy_volume: 0.0,
            sell_volume: 0.0,
            tick_count: 0,
        }
    }

    pub fn process_tick(&mut self, side: &str, volume: f64) -> (f64, f64) {
        match side {
            "buy" => {
                self.cvd += volume;
                self.buy_volume += volume;
            }
            "sell" => {
                self.cvd -= volume;
                self.sell_volume += volume;
            }
            _ => {}
        }

        self.tick_count += 1;

        let total = self.buy_volume + self.sell_volume;
        let ofi = if total > 0.0 {
            (self.buy_volume - self.sell_volume) / total
        } else {
            0.0
        };

        if self.tick_count % 100 == 0 {
            self.buy_volume = 0.0;
            self.sell_volume = 0.0;
        }

        (self.cvd, ofi)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn cvd_accumulates() {
        let mut s = ProcessingState::new();
        assert_eq!(s.process_tick("buy", 1.0), (1.0, 1.0));
        assert_eq!(s.process_tick("sell", 0.5), (0.5, 1.0 / 3.0));
    }

    #[test]
    fn ofi_resets_every_100_ticks() {
        let mut s = ProcessingState::new();
        for _ in 0..100 {
            s.process_tick("buy", 1.0);
        }
        let (_, ofi) = s.process_tick("sell", 1.0);
        assert_eq!(ofi, -1.0);
    }
}
